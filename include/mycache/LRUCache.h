//
// Created by py on 26-1-22.
//
// ============================ 学习注释（不含实现代码）============================
// 这个文件是：LRU 缓存策略的实现位置（未来会放类 LRUCache）。
//
// LRU（Least Recently Used）是什么？
// - 当缓存满了，需要淘汰旧数据时：优先淘汰“最近最久没被访问过”的那条。
// - 直觉：最近用过的更可能马上还会用；很久没用过的更可能可以丢。
//
// 典型实现需要两类结构（后续你会亲手写）：
// 1) 一个“访问顺序表”用来记录新旧（常见：双向链表 list）
//    - 表头：最近访问
//    - 表尾：最久未访问（要淘汰）
// 2) 一个“快速定位表”用 key 快速找到数据和它在链表的位置（常见：unordered_map）
//
// 你最终要在这里完成什么？（后续步骤会一点点来）
// - 定义 LRUCache<K, V> 类
// - 它会“实现/遵守” ICache<K, V> 这个统一接口
// - 先做单线程版本，再在之后加锁做成线程安全
//
// 你完成后应该能支持这些最小行为：
// - put: 写入（如果 key 已存在则更新，同时算作一次访问）
// - get: 读取（命中则更新访问顺序，未命中返回空）
// - erase: 删除
// - 容量满时自动淘汰
// ==============================================================================

#ifndef CACHESTUDY_LRUCACHE_H
#define CACHESTUDY_LRUCACHE_H

#include "mycache/ICache.h"

#include <optional>
#include <cstddef>
#include <list>
#include <unordered_map>

namespace mycache {
    // 这里之后会放：LRUCache<K, V> 的声明与实现。
    // 现在先留空，等你先把 ICache 的接口学会、写好。
    template<class K, class V>
    class LRUCache : public ICache<K, V> {
    private:
        size_t capacity_; // 最大容量
        size_t size_;     // 现存大小
        std::list<std::pair<K, V>> lst_;
        std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> mp_;
    public:
        // LRUCache(); // 禁止

        explicit LRUCache(size_t capacity = 10);

        bool put(const K &key, const V &value) override;

        std::optional<V> get(const K &key) override;

        bool erase(const K &key) override;

        // 这里的[[nodiscard]] 的作用就是：提醒调用者别把返回值丢掉。
        [[nodiscard]] size_t size() const override;

        [[nodiscard]] size_t capacity() const override;

        ~LRUCache() override = default;
    };

//    template<class K, class V>
//    LRUCache<K, V>::LRUCache() {
//        this->size_ = 0;
//        this->capacity_ = 10;
//    }

    template<class K, class V>
    LRUCache<K, V>::LRUCache(size_t capacity):capacity_(capacity), size_(0) {}

    template<class K, class V>
    bool LRUCache<K, V>::put(const K &key, const V &value) {
        if (mp_.contains(key)) {
            mp_.
        } else {

        }
        if (size_ < capacity_) {
            if (mp_.contains(key)) {

            } else {
                lst_.push_front(value);
                mp_[key] = lst_.begin();
                size_++;
            }

        } else {

        }
        return true;
    }

    template<class K, class V>
    std::optional<V> LRUCache<K, V>::get(const K &key) {
        auto mapIt = mp_.find(key);
        if (size_ == 0 || mapIt == mp_.end()) {
            return std::nullopt;
        } else {
            auto nodeIt = mapIt->second; // list的迭代器
            V value = it->second;
            lst_.splice(lst_.begin(), lst_, it);
            return value;
        }
    }

    template<class K, class V>
    bool LRUCache<K, V>::erase(const K &key) {
        if (size_ == 0) {
            return false;
        } else {
            size_--;
            return true;
        }
    }

    template<class K, class V>
    size_t LRUCache<K, V>::size() const {
        return size_;
    }

    template<class K, class V>
    size_t LRUCache<K, V>::capacity() const {
        return capacity_;
    }

} // namespace mycache

#endif //CACHESTUDY_LRUCACHE_H
