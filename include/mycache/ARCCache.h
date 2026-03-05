//
// Created by py on 26-3-4.
//

#ifndef CACHESTUDY_ARCCACHE_H
#define CACHESTUDY_ARCCACHE_H

#include "mycache/ICache.h"
#include <list>
#include <unordered_map>

namespace mycache {
    template<class K, class V>
    class ARCCache : public ICache<K, V> {
    private:
        // 注意 capacity 为 0 时是缓存禁用
        size_t capacity_;
        size_t size_;
        // 重要参数
        size_t p_;
        // t1,t2 真实存储的数据
        std::list<std::pair<K, V>> t1_;
        std::list<std::pair<K, V>> t2_;
        std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> t1_mp_;
        std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> t2_mp_;
        // ghost
        std::list<K> b1_;
        std::list<K> b2_;

        std::unordered_map<K, typename std::list<K>::iterator> b1_mp_;
        std::unordered_map<K, typename std::list<K>::iterator> b2_mp_;

    public:
        bool put(const K &key, const V &value) override;

        std::optional<V> get(const K &key) override;

        bool erase(const K &key) override;

        [[nodiscard]] size_t size() const override;

        [[nodiscard]] size_t capacity() const override;
    };

    template<class K, class V>
    size_t ARCCache<K, V>::capacity() const {
        return capacity_;
    }

    template<class K, class V>
    size_t ARCCache<K, V>::size() const {
        return size_;
    }

    template<class K, class V>
    bool ARCCache<K, V>::erase(const K &key) {
        return false;
    }

    template<class K, class V>
    std::optional<V> ARCCache<K, V>::get(const K &key) {
        return std::optional<V>();
    }

    template<class K, class V>
    bool ARCCache<K, V>::put(const K &key, const V &value) {
        if (capacity_ == 0) return false;
        auto it1 = t1_mp_.find(key);
        auto it2 = t2_mp_.find(key);

        // 需要考虑已满淘汰
        if (it1 == t1_mp_.end() || it2 == t2_mp_.end()) {
            if (it1 == t1_mp_.end() && it2 == t2_mp_.end()) {
                // 奇奇怪怪的，如果有这个情况，那就是代码有问题了
            } else if (it1 == t1_mp_.end()) {
                // 不在t1就在t2了
                // t2不用考虑ghost
                std::pair<K, V> temp(it2->second);
                t2_.erase(it2);
                t2_.push_front(temp);
                t2_mp_[key] = t2_.front();
            } else {
                // 不在t2就在t1
                // 有可能t2的不够了，需要进行淘汰
                std::pair<K, V> temp(it1->second);
                t1_mp_.erase(key);
                t1_.erase(it1);
                t2_.push_front(temp);
                t2_mp_[key] = t2_.front();
            }
        } else {
            // 两个都不在 , 需要淘汰

        }

    }

}
#endif //CACHESTUDY_ARCCACHE_H
