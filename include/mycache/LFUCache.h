#ifndef CACHESTUDY_LFUCACHE_H
#define CACHESTUDY_LFUCACHE_H
// Step 3\) LFU 推荐拆解（不直接改你现有代码，只列任务清单）
//
// 1. 新增 `include/mycache/LFUCache.h`（先做单线程，先不加 mutex）
//    - 数据结构：
//      - unordered\_map<K, Node> nodes; Node{V value; int freq; list<K>::iterator it;}
//      - unordered\_map<int, list<K>> freq\_list;  // 每个频次一个 key 列表，表头最新
//      - int minFreq;
//    - 语义：
//      - get 命中：freq++，把 key 从旧 freq_list 移到新 freq_list 表头
//      - put 新 key：容量满时淘汰 minFreq 的 freq_list.back()
//      - put 已存在：更新 value，且算一次访问（等同 get 一样提升 freq）
//
// 2. 在 `tests/test_lru.cpp` 旁边新增 `tests/test_lfu.cpp`（或把 LFU 测试也加进同一个测试可执行程序）
//    - 先写 4 个最小测试：
//      - put/get 基本命中
//      - 容量满时按 freq 淘汰（同 freq 时按“最久未使用于该 freq bucket”淘汰）
//      - 覆盖写入会提升 freq
//      - capacity==0 语义一致
//
// 3. LFU 单线程测试通过后，再给 LFU 加“单锁线程安全”\(+ Debug 不变量\)，模式照搬你 LRU 的做法
//
// 4. 最后再考虑 `2\) 分片`：做一个 `ShardedCache<K,V,Policy>` 或者分别 `ShardedLRUCache`/`ShardedLFUCache`




#include "mycache/ICache.h"
#include <cstddef>
#include <unordered_map>
#include <list>
#include <mutex>
#include <cassert>
#include <optional>

namespace mycache {
    template<class K, class V>
    class LFUCache : public ICache<K, V> {
    private:
        // 基本数据结构
        struct Node {
            V value;
            int freq;
            typename std::list<K>::iterator it; // 指向 freq_list_[freq] 中该 key 的位置
        };

        std::unordered_map<K, Node> nodes_;               // key -> {value,freq,it}

        std::unordered_map<int, std::list<K>> freq_list_; // freq -> keys (front=MRU, back=LRU)

        int minFreq_;

        size_t size_;// 已经存储的大小

        size_t capacity_;// 可以存储的大小
    private:
        mutable std::mutex mtx_;
    private:
        void check_invariants_() const{
            assert(size_ == nodes_.size());

            assert(size_ <= capacity_);
            if (capacity_ == 0) {
                assert(size_ == 0);
                assert(nodes_.empty());
                assert(freq_list_.empty());
            }
            size_t sum = 0;
            for (const auto &item: freq_list_){
                sum += item.second.size();
            }
            assert(size_ == sum);


        }

    private:
        void touch_(const K &key) {
            auto itNode = nodes_.find(key);
            assert(itNode != nodes_.end()); // 理应是找得到的

            Node &node = itNode->second;
            const int f = node.freq;

            auto itBucket = freq_list_.find(f);
            assert(itBucket != freq_list_.end());

            // 从旧频次桶删除 key
            itBucket->second.erase(node.it);

            // 旧桶空了那就需要更新 minFreq_
            if (itBucket->second.empty()) {
                freq_list_.erase(itBucket);
                if (minFreq_ == f) {
                    minFreq_ = f + 1;
                }
            }

            // 插入到新频次桶表头
            const int nf = f + 1;
            auto &newList = freq_list_[nf];
            newList.push_front(key);

            node.freq = nf;
            node.it = newList.begin();
        }

    public:
        explicit LFUCache(size_t capacity) : size_(0), capacity_(capacity), minFreq_(0) {}

        bool put(const K &key, const V &value) override;

        bool erase(const K &key) override;

        std::optional<V> get(const K &key) override;

        [[nodiscard]] size_t size() const override;

        [[nodiscard]] size_t capacity() const override;

        ~LFUCache() override = default;
    };

    template<class K, class V>
    bool LFUCache<K, V>::put(const K &key, const V &value) {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        if (capacity_ == 0) return false;

        auto it = nodes_.find(key);
        if (it != nodes_.end()) {
            it->second.value = value;
            touch_(key);
            return true;
        }

        // 需要进行淘汰
        if (size_ == capacity_) {
#ifndef NDEBUG
            size_t steps = 0;
            const size_t limit = size_ + 1; // 理论上不会超过当前元素数+1
#endif
            // 迫不得已的一个方法，就是因为erase导致的
            while (freq_list_.find(minFreq_) == freq_list_.end()) {
                ++minFreq_;
#ifndef NDEBUG
                ++steps;
                assert(steps <= limit);
#endif
            }

            auto itMin = freq_list_.find(minFreq_);
            assert(itMin != freq_list_.end());
            auto &lst = itMin->second;

            const K victim = lst.back(); // minFreq 桶内的 LRU
            lst.pop_back();
            if (lst.empty()) {
                freq_list_.erase(itMin);
            }
            nodes_.erase(victim);
            --size_;
        }

        minFreq_ = 1;
        auto &l1 = freq_list_[1];
        l1.push_front(key);

        nodes_.emplace(key, Node{value, 1, l1.begin()});
        ++size_;

        check_invariants_();
        return true;
    }

    template<class K, class V>
    bool LFUCache<K, V>::erase(const K &key) {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        if (capacity_ == 0) return false;

        auto it = nodes_.find(key);
        if (it == nodes_.end()) return false;

        const int f = it->second.freq;
        auto itBucket = freq_list_.find(f); // 找到对应的桶
        assert(itBucket != freq_list_.end());

        itBucket->second.erase(it->second.it); // 桶内删除
        // 桶空删桶
        if (itBucket->second.empty()) {
            freq_list_.erase(itBucket);
        }
        // nodes表删除 key
        nodes_.erase(it);
        --size_;
        // 缓存为空，最小的Freq也应该改成 0
        if (size_ == 0) {
            minFreq_ = 0;
        }

        check_invariants_();
        return true;
    }

    template<class K, class V>
    std::optional<V> LFUCache<K, V>::get(const K &key) {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        if (capacity_ == 0) return std::nullopt;
        auto it = nodes_.find(key);
        if (it == nodes_.end()) return std::nullopt;
        auto val = it->second.value;
        touch_(key);
        check_invariants_();
        return val;
    }

    template<class K, class V>
    size_t LFUCache<K, V>::size() const {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        return size_;
    }

    template<class K, class V>
    size_t LFUCache<K, V>::capacity() const {
        std::lock_guard<std::mutex> lockGuard(mtx_);
        return capacity_;
    }
}// namespace mycache


#endif //CACHESTUDY_LFUCACHE_H