//
// Created by py on 26-1-22.
//
/*
 * ============================ LRUCache (Library Contract) ============================
 *
 * 这是一个基于 LRU（Least Recently Used, 最近最少使用）策略的本地内存缓存。
 * 设计目标：
 * - O(1) 平均复杂度的 put/get/erase（借助 list + unordered_map）
 * - 行为语义明确、边界情况安全（不会崩溃），便于未来扩展到 LFU/ARC
 *
 * ---------------------------- 核心数据结构（实现思路） ----------------------------
 * - lst_ : std::list<std::pair<K,V>>
 *   维护访问顺序：
 *   - lst_.front()  : 最近使用（Most Recently Used, MRU）
 *   - lst_.back()   : 最久未使用（Least Recently Used, LRU，满时被淘汰）
 *
 * - mp_ : std::unordered_map<K, list::iterator>
 *   让我们能 O(1) 找到 key 对应的链表节点，并在命中时把节点移动到表头（splice）。
 *
 * ---------------------------- 行为语义（非常重要） ----------------------------
 * 1) capacity 语义
 * - capacity_ == 0 表示“禁用缓存”：
 *   - put(...)   永远返回 false（不写入）
 *   - get(...)   永远返回 std::nullopt（永远 miss）
 *   - erase(...) 永远返回 false（不删除）
 *   - size()     永远返回 0
 *   - 内部 lst_ / mp_ 应保持为空（不应发生任何插入/淘汰动作）
 *
 * 2) put(key, value)
 * - 如果 key 不存在：
 *   - 若 size_ < capacity_：直接插入到表头（成为 MRU），size_++
 *   - 若 size_ == capacity_：先淘汰表尾（LRU），再插入表头（MRU）
 * - 如果 key 已存在：
 *   - 更新该节点的 value
 *   - 并把该节点移动到表头（这次 put 也算一次访问）
 *
 * 3) get(key)
 * - 未命中：返回 std::nullopt
 * - 命中：返回 value，并把该节点移动到表头（算一次访问）
 *
 * 4) erase(key)
 * - key 不存在：返回 false
 * - key 存在：从 mp_ 与 lst_ 中都删除，size_--，返回 true
 *
 * ---------------------------- 不变量（写库一定要有） ----------------------------
 * 在任何一次操作完成后，都应当成立：
 * - size_ == lst_.size()
 * - size_ == mp_.size()
 * - size_ <= capacity_
 *
 * （建议 Debug 模式下用 assert 检查这些不变量，后续加锁/优化/重构时很好用）
 *
 * ---------------------------- 复杂度说明 ----------------------------
 * 平均时间复杂度：
 * - put/get/erase : O(1) average（unordered_map + list splice/erase）
 * 空间复杂度：
 * - O(capacity_)
 *
 * ---------------------------- 注意事项 ----------------------------
 * - 该实现目前是单线程版本（@TODO后续 Step B 会加 mutex 实现线程安全）
 * - get/put 会改变访问顺序，因此它们不是 const 成员函数
 * ==============================================================================
 */

#ifndef CACHESTUDY_LRUCACHE_H
#define CACHESTUDY_LRUCACHE_H

#include "mycache/ICache.h"

#include <optional>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <cassert>
#include <mutex>

namespace mycache {
    // 这里之后会放：LRUCache<K, V> 的声明与实现。
    // 现在先留空，等你先把 ICache 的接口学会、写好。
    //
    template<class K, class V>
    class LRUCache : public ICache<K, V> {
    private:
        // 各个成员变量的互斥量
//        mutable std::mutex mt_capacity_;
//        mutable std::mutex mt_size_;
//        std::mutex mt_lst_;
//        std::mutex mt_mp_;
        mutable std::mutex mut_;

    private:
        size_t capacity_; // 最大容量
        size_t size_;     // 现存大小
        std::list<std::pair<K, V>> lst_;  // 缓存链表
        std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> mp_; // 哈希表快速定位 key 在缓存链表的位置
    public:
        // LRUCache(); // 禁止

        /*
         * @capacity ： 默认 10；若显式传入 0，则表示禁用缓存
         * */
        explicit LRUCache(size_t capacity = 10);

        /*
         * @key : 推送的键值的键
         * @value : 推送的键值的值
         * @return : 当写入成功返回true，写入不成功返回false
         * 特别的：capacity = 0时，始终返回false
         * */
        bool put(const K &key, const V &value) override;

        /*
         * @key : 推送的键值的键
         *
         * 特别的：capacity = 0时，始终返回std::nullopt;
         * */
        std::optional<V> get(const K &key) override;

        /*
         * @key : 推送的键值的键
         *
         * 特别的：capacity = 0时，始终返回false;
         * */
        bool erase(const K &key) override;

        // 这里的[[nodiscard]] 的作用就是：提醒调用者别把返回值丢掉。
        // 返回 size_
        [[nodiscard]] size_t size() const override;

        // 返回 capacity_
        [[nodiscard]] size_t capacity() const override;

        // 重构析构函数
        ~LRUCache() override = default;

    private:
#ifndef NDEBUG

        void check_invariants_() const {
            // 验证size_是否存在问题
            assert(size_ == lst_.size());
            assert(size_ == mp_.size());
            assert(lst_.size() == mp_.size());
            // capacity == 0 的全禁用模式
            if (capacity_ == 0){
                assert(size_ == 0);
                assert(lst_.empty());
                assert(mp_.empty());
            }

            // assert(capacity_ == 0 && size_ == 0)
            assert(size_ <= capacity_);

            // size == 0其他的 lst_ 和 mp_ 理应不存东西
            if (size_ == 0) {
                assert(lst_.empty());
                assert(mp_.empty());
            }
        }

#else
        void check_invariants_()const{}

#endif
    };


//    template<class K, class V>
//    LRUCache<K, V>::LRUCache() {
//        this->size_ = 0;
//        this->capacity_ = 10;
//    }

    template<class K, class V>
    LRUCache<K, V>::LRUCache(size_t capacity): capacity_(capacity), size_(0) {}

    template<class K, class V>
    bool LRUCache<K, V>::put(const K &key, const V &value) {

        std::lock_guard<std::mutex> lock_capacity(mut_);
        if (capacity_ == 0)return false;
        auto mapId = mp_.find(key);
        // 找不到
        if (mapId == mp_.end()) {
            // 容量充足，加链头即可
            if (size_ < capacity_) {
                lst_.push_front({key, value});
                size_++;
            } else {
                // 满时 size_ 不变
                auto lrb = lst_.rbegin();
                mp_.erase(lrb->first);
                lst_.pop_back();
                lst_.push_front({key, value});
            }
            mp_[key] = lst_.begin();
        } else {
            // key存在，那么更新到链头，并更新值
            lst_.splice(lst_.begin(), lst_, mapId->second);
            lst_.begin()->second = value;
        }
        check_invariants_();


        return true;
    }

    template<class K, class V>
    std::optional<V> LRUCache<K, V>::get(const K &key) {

        std::lock_guard<std::mutex> lock_capacity(mut_);
        if (capacity_ == 0) return std::nullopt;
        auto mapIt = mp_.find(key);
        // 肯定是看找不找得到，找不到就不管是否现在是空都可以直接返回了，懒得用那个
        if (mapIt == mp_.end()) {
            return std::nullopt;
        } else {
            auto nodeIt = mapIt->second; // list的迭代器
            lst_.splice(lst_.begin(), lst_, nodeIt);
            check_invariants_();

            return std::optional<V>(lst_.begin()->second);
        }


    }

    template<class K, class V>
    bool LRUCache<K, V>::erase(const K &key) {

        std::lock_guard<std::mutex> lock_mp(mut_);

        if (capacity_ == 0 || mp_.empty()) {
            return false;
        } else {
            auto mapId = mp_.find(key);
            if (mapId == mp_.end()) return false;
            auto nodeId = mapId->second;
            lst_.erase(nodeId);
            mp_.erase(key);
            size_--;
            check_invariants_();
            return true;
        }


    }

    template<class K, class V>
    size_t LRUCache<K, V>::size() const {
        std::lock_guard<std::mutex> lock_size(mut_);
        return size_;
    }

    template<class K, class V>
    size_t LRUCache<K, V>::capacity() const {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        return capacity_;
    }


} // namespace mycache

#endif //CACHESTUDY_LRUCACHE_H
