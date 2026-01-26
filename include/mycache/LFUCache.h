#ifndef CACHESTUDY_LFUCACHE_H
#define CACHESTUDY_LFUCACHE_H

// =============================================================================
// LFUCache.h
// =============================================================================
// 这是一个“库化风格”的 LFU（Least Frequently Used）缓存实现：
//
// 特性：
// 1) 策略：
//    - 优先淘汰“访问频次 freq 最小”的条目。
//    - 若多个条目 freq 相同，则在该 freq 桶内按 LRU 规则淘汰：淘汰最久未使用（桶内 back）。
//
// 2) 复杂度（典型/均摊）：
//    - get/put/erase：期望 O(1)（依赖于 unordered_map 的均摊常数时间）。
//    - 这是经典的“哈希 + 链表分桶”做法。
//
// 3) 线程安全：
//    - 采用“单把大锁”（mtx_）保护所有公开 API：put/get/erase/size/capacity。
//    - 这保证正确性与易维护性；性能优化（例如分片/读写锁）属于后续扩展。
//
// 4) 重要语义约定：
//    - capacity == 0 表示“禁用缓存”。
//      - put(...) 返回 false
//      - get(...) 返回 std::nullopt
//      - erase(...) 返回 false
//      - size() 恒为 0
//
// 5) 不变量（Debug-only）：
//    - size_ 与 nodes_、freq_list_ 中的元素数量保持一致。
//    - size_ <= capacity_
//    - size_==0 时 minFreq_==0 且 freq_list_ 为空
//    - size_>0 时 minFreq_ 对应的桶必须存在（freq_list_ 中能 find 到）
//
// 设计提示：
// - 这是一个“模板 + header-only”实现：模板类的成员函数定义放在头文件里，
//   否则容易出现链接错误（找不到模板实例化的定义）。
// =============================================================================

#include "mycache/ICache.h"
#include <cstddef>
#include <unordered_map>
#include <list>
#include <mutex>
#include <cassert>
#include <optional>
#include <algorithm>
#include <cstdint>

namespace mycache {
    template<class K, class V>
    class LFUCache : public ICache<K, V> {
    private:
        // Node：单个 key 在缓存中的元信息。
        // - value：保存的值
        // - freq ：访问频次（命中 get/覆盖 put 都算一次访问，会让 freq++）
        // - it   ：指向 freq_list_[freq] 这个链表中 key 的位置（用于 O(1) 删除/移动）
        struct Node {
            V value;
            int freq;
            typename std::list<K>::iterator it; // 指向 freq_list_[freq] 中该 key 的位置
        };

        // 关键数据结构 1：key -> Node（用来 O(1) 定位某个 key 是否存在、其频次是多少、以及 list 位置）
        std::unordered_map<K, Node> nodes_;

        // 关键数据结构 2：freq -> list<key>
        // - 每个频次一个桶（bucket）
        // - list 的 front 表示“该频次里最近访问（MRU）”，back 表示“该频次里最久未访问（LRU）”
        // - 淘汰时：从 minFreq_ 的桶里取 back()
        std::unordered_map<int, std::list<K>> freq_list_;

        // minFreq_：当前缓存中“最小访问频次”。
        // - 淘汰时用它直接定位桶，实现 O(1) 级别的淘汰定位。
        // - 约定：size_==0 时 minFreq_==0；size_>0 时 minFreq_ 必须能在 freq_list_ 中找到。
        int minFreq_;

        // size_：当前缓存元素个数（与 nodes_.size() 一致；单独存一份便于 O(1) 返回）
        size_t size_;

        // capacity_：最大容量。约定 capacity_==0 时表示禁用缓存。
        size_t capacity_;

    private:
        // 单把大锁，保护所有状态（nodes_/freq_list_/minFreq_/size_）。
        // mutable：允许在 const 成员函数（size/capacity/check_invariants_）中加锁。
        mutable std::mutex mtx_;

    private:
        // Debug-only：用于尽早发现“map/list 不同步”这类隐蔽 bug。
        // 注意：本函数假定调用时已经持有 mtx_。
        void check_invariants_() const {
            assert(size_ == nodes_.size());
            assert(size_ <= capacity_);

            if (capacity_ == 0) {
                // 禁用缓存模式：内部结构不应存任何元素
                assert(size_ == 0);
                assert(nodes_.empty());
                assert(freq_list_.empty());
            }

            // 统计所有桶里的 key 总数，必须等于 size_
            size_t sum = 0;
            for (const auto &item: freq_list_) {
                sum += item.second.size();
            }
            assert(size_ == sum);

            // minFreq_ 的有效性（强不变量）
            if (size_ == 0) {
                assert(freq_list_.empty());
                assert(nodes_.empty());
                assert(minFreq_ == 0);
            } else {
                assert(!freq_list_.empty());
                assert(freq_list_.find(minFreq_) != freq_list_.end());
            }

        }

    private:
        // touch_(key)：将 key 的访问频次 +1，并把 key 移动到新频次桶的表头（MRU）。
        // - 只在持有 mtx_ 的情况下调用（因此此函数自身不加锁）。
        // - 这是 LFU 的核心“命中更新”逻辑。
        void touch_(const K &key) {
            auto itNode = nodes_.find(key);
            assert(itNode != nodes_.end()); // 只在命中时调用，理论上一定存在

            Node &node = itNode->second;
            const int f = node.freq;

            auto itBucket = freq_list_.find(f);
            assert(itBucket != freq_list_.end());

            // 1) 先从旧频次桶里移除 key（O(1)，因为有 iterator）
            itBucket->second.erase(node.it);

            // 2) 若旧桶空了，需要删桶；并且若这个桶就是 minFreq_，则 minFreq_ 进入下一档
            //    注意：这里直接 minFreq_=f+1 是安全的，因为本函数稍后一定会把 key 放进 nf 桶，
            //    从而保证 minFreq_ 对应桶存在（至少 nf 桶会被创建）。
            if (itBucket->second.empty()) {
                freq_list_.erase(itBucket);
                if (minFreq_ == f) {
                    minFreq_ = f + 1;
                }
            }

            // 3) 插入到新频次桶表头（MRU）
            const int nf = f + 1;
            auto &newList = freq_list_[nf];
            newList.push_front(key);

            // 4) 回写 Node 元信息
            node.freq = nf;
            node.it = newList.begin();
        }

    public:
        // 构造：指定容量。
        // - capacity==0 表示禁用缓存（这是一种“显式且可测试”的行为约定）。
        explicit LFUCache(size_t capacity) : size_(0), capacity_(capacity), minFreq_(0) {}

        // put：写入/更新。
        // - key 不存在：插入新条目（必要时淘汰）。新条目 freq=1。
        // - key 已存在：更新 value，并视为一次访问（freq++）。
        bool put(const K &key, const V &value) override;

        // erase：按 key 删除。
        // - 成功删除返回 true
        // - key 不存在返回 false
        bool erase(const K &key) override;

        // get：读（命中会提升 freq，并更新桶内 MRU 顺序）
        std::optional<V> get(const K &key) override;

        // size/capacity：查询。
        // [[nodiscard]]：提示调用者不要忽略返回值（尤其是 size/capacity 这类“查询函数”）。
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
            // 覆盖写：更新 value，并算一次访问（调用 touch_ 让 freq++）
            it->second.value = value;
            touch_(key);
            check_invariants_();
            return true;
        }

        // 插入新 key：若空间不足则需要淘汰
        if (size_ == capacity_) {

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

        // 新条目插入：freq=1，且 minFreq_ 必然回到 1
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

        itBucket->second.erase(it->second.it); // 桶内删除（O(1)）

        // 桶空则删桶；若删的是 minFreq_ 桶，需要把 minFreq_ 修正到“当前存在的最小桶”
        if (itBucket->second.empty()) {
            freq_list_.erase(itBucket);

            if (freq_list_.empty()) {
                // 注意：此时 nodes_ 里还没 erase(it)，size_ 也还没减。
                // 但 freq_list_ 已空，说明这是最后一个元素所在的桶。
                minFreq_ = 0;
            } else if (f == minFreq_) {
                int minF = INT32_MAX;
                for (const auto &item: freq_list_) {
                    minF = std::min(item.first, minF);
                }
                minFreq_ = minF;
            }
        }

        nodes_.erase(it);
        --size_;
        if (size_ == 0) {
            // 删除最后一个元素后，显式回到“空缓存”态
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

        // 先拷贝 value，再 touch_（touch_ 会改动 nodes_ 内部结构）
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

