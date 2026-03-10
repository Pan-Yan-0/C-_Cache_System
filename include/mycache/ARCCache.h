//
// Created by py on 26-3-4.
//

#ifndef CACHESTUDY_ARCCACHE_H
#define CACHESTUDY_ARCCACHE_H

#include "mycache/ICache.h"
#include <list>
#include <unordered_map>
#include <cassert>
#include <mutex>
namespace mycache {
    /*
     * 核心约束：
     * t1+t2 <= capacity
     * t1+t2+b1+b2 <= 2 * capacity
     * */
    template<class K, class V>
    class ARCCache : public ICache<K, V> {
    private:
        // 注意 capacity 为 0 时是缓存禁用
        size_t capacity_;
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

        mutable std::mutex mut_;
    public:
        ARCCache(size_t capacity) : capacity_(capacity), p_(capacity / 2) {};

        bool put(const K &key, const V &value) override;

        std::optional<V> get(const K &key) override;

        bool erase(const K &key) override;

        /*
         * 前置触发条件：当且仅当 |t1| + |t2| == capacity
         * 具体作用：将当前t1/t2中的一个节点弹出并且将其变成ghost节点插入到b1/b2中
         * 后置条件: |t1| + |t2| 减一
         *          t1_mp_/t2_mp_ 与 t1_/t2_ 同步（删掉淘汰 key）
         *          不改变 p_（p 的调整只在 B1/B2 hit 做）
         * */
        void replace();

        void repeat_ghost();

        [[nodiscard]] size_t size() const override;

        [[nodiscard]] size_t capacity() const override;

    private:
#ifndef NDEBUG

        [[maybe_unused]] void check_key_in_mp_only_(const K &key) const {
            int c = 0;
            c += t1_mp_.find(key) != t1_mp_.end() ? 1 : 0;
            c += t2_mp_.find(key) != t2_mp_.end() ? 1 : 0;
            c += b1_mp_.find(key) != b1_mp_.end() ? 1 : 0;
            c += b2_mp_.find(key) != b2_mp_.end() ? 1 : 0;
            assert(c == 1);
        }

        [[maybe_unused]] void check_key_in_mp2_but_it_error(const K &key, const V &value) const {
            auto it = t2_mp_.find(key);
            assert(it != t2_mp_.end());
            assert(it->second->first == key);
            assert(it->second->second == value);
        }

        [[maybe_unused]] void check_key_in_mp1_but_it_error(const K &key, const V &value) const {
            auto it = t1_mp_.find(key);
            assert(it != t1_mp_.end());
            assert(it->second->first == key);
            assert(it->second->second == value);
        }

#else
        void check_key_in_mp_only_(const K&)const{}
        void check_key_in_mp2_but_it_error(const K &, const V &)const{}
        void check_key_in_mp1_but_it_error(const K &, const V &)const{}
#endif
    };

    template<class K, class V>
    void ARCCache<K, V>::repeat_ghost() {
        if (t1_.size() + t2_.size() + b1_.size() + b2_.size() <= 2 * capacity_) return;
        while (t1_.size() + t2_.size() + b1_.size() + b2_.size() > 2 * capacity_) {
            // 防御写法
            assert(!b1_.empty() || !b2_.empty());
            // if (t1_.empty() && t2_.empty()) break;
            if (b1_.size() > b2_.size()) {
                b1_mp_.erase(b1_.back());
                b1_.pop_back();
            } else {
                b2_mp_.erase(b2_.back());
                b2_.pop_back();
            }
        }
    }


    template<class K, class V>
    void ARCCache<K, V>::replace() {
#ifndef NDEBUG
        assert(t1_.size() + t2_.size() == capacity_);
        assert(capacity_ > 0);
        assert(!t1_.empty() || !t2_.empty());
#endif
        auto pop_t1 = [&]() -> void {
            const K &key = t1_.back().first;
            auto it = b1_mp_.find(key);

            if (it != b1_mp_.end()) {
                b1_.erase(it->second);
                b1_mp_.erase(key);
            }
            b1_.push_front(key);
            b1_mp_[key] = b1_.begin();
            t1_mp_.erase(key);
            t1_.pop_back();

        };
        auto pop_t2 = [&]() -> void {
            const K &key = t2_.back().first;
            auto it = b2_mp_.find(key);

            if (it != b2_mp_.end()) {
                b2_.erase(it->second);
                b2_mp_.erase(key);
            }
            b2_.push_front(key);
            b2_mp_[key] = b2_.begin();
            t2_mp_.erase(key);
            t2_.pop_back();
        };
        if (!t1_.empty() && t1_.size() > p_) {
            pop_t1();
        } else {
            if (t2_.empty()) {
                pop_t1();
            } else {
                pop_t2();
            }
        }
    }

    template<class K, class V>
    size_t ARCCache<K, V>::capacity() const {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        return capacity_;
    }

    template<class K, class V>
    size_t ARCCache<K, V>::size() const {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        return t1_.size() + t2_.size();
    }

    template<class K, class V>
    bool ARCCache<K, V>::erase(const K &key) {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        if (capacity_ == 0) return false;
        bool removed = false;
        auto it1 = t1_mp_.find(key);
        if (it1 != t1_mp_.end()) {
            // t1 hit
            t1_.erase(it1->second);
            t1_mp_.erase(key);
            removed = true;
        }
        auto it2 = t2_mp_.find(key);
        if (it2 != t2_mp_.end()) {
            // t2 hit
            t2_.erase(it2->second);
            t2_mp_.erase(key);
            removed = true;
        }
        auto it3 = b1_mp_.find(key);
        if (it3 != b1_mp_.end()) {
            // b1 hit
            b1_.erase(it3->second);
            b1_mp_.erase(key);
            removed = true;
        }
        auto it4 = b2_mp_.find(key);
        if (it4 != b2_mp_.end()) {
            // b2 hit
            b2_.erase(it4->second);
            b2_mp_.erase(key);
            removed = true;
        }
        return removed;
    }

    template<class K, class V>
    std::optional<V> ARCCache<K, V>::get(const K &key) {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        if (capacity_ == 0) return std::nullopt;
        auto it1 = t1_mp_.find(key);
        auto it2 = t2_mp_.find(key);
        auto it3 = b1_mp_.find(key);
        auto it4 = b2_mp_.find(key);
        if (it1 != t1_mp_.end()) {
            // t1 hit
            t2_.splice(t2_.begin(), t1_, it1->second);
            t1_mp_.erase(key);
            t2_mp_[key] = t2_.begin();
            check_key_in_mp_only_(key);
            return std::optional<V>(t2_.begin()->second);
        } else if (it2 != t2_mp_.end()) {
            // t2 hit
            t2_.splice(t2_.begin(), t2_, it2->second);
            check_key_in_mp_only_(key);
            return std::optional<V>(t2_.begin()->second);
        } else if (it3 != b1_mp_.end()) {
            // b1 hit
            assert(!b1_.empty());
            size_t delta = std::max<size_t>(1, b2_.size() / b1_.size());
            p_ = std::min(capacity_, p_ + delta);
            b1_.splice(b1_.begin(), b1_, it3->second);
            check_key_in_mp_only_(key);
            return std::nullopt;
        } else if (it4 != b2_mp_.end()) {
            // b2 hit
            assert(!b2_.empty());
            size_t delta = std::max<size_t>(1, b1_.size() / b2_.size());
            p_ = delta > p_ ? 0 : p_ - delta;
            b2_.splice(b2_.begin(), b2_, it4->second);
            check_key_in_mp_only_(key);
            return std::nullopt;
        } else {
            // miss
            return std::nullopt;
        }

    }

    // 命中直接进行修改key的value值（MRU策略）
    template<class K, class V>
    bool ARCCache<K, V>::put(const K &key, const V &value) {
        std::lock_guard<std::mutex> lock_capacity(mut_);
        if (capacity_ == 0) return false;
        auto it1 = t1_mp_.find(key);
        auto it2 = t2_mp_.find(key);
        auto it3 = b1_mp_.find(key);
        auto it4 = b2_mp_.find(key);

        if (it1 != t1_mp_.end()) {
            // 更新值
            it1->second->second = value;
            t2_.splice(t2_.begin(), t1_, it1->second);
            t1_mp_.erase(key);
            t2_mp_[key] = t2_.begin();
#ifndef NDEBUG
            check_key_in_mp2_but_it_error(key, value);
#endif
        } else if (it2 != t2_mp_.end()) {
            // 更新值
            it2->second->second = value;
            // 这个就并没有任何的问题
            t2_.splice(t2_.begin(), t2_, it2->second);
#ifndef NDEBUG
            check_key_in_mp2_but_it_error(key, value);
#endif
        } else if (it3 != b1_mp_.end()) {
            // 不在t1,t2 ， 在ghost表 b1 中
            // 这里会存在除 0异常，但是逻辑上已经避免了
            size_t delta = std::max<size_t>(1, b2_.size() / b1_.size());
            p_ = std::min(capacity_, p_ + delta);
            b1_.erase(it3->second);
            b1_mp_.erase(key);
            if (t1_.size() + t2_.size() == capacity_) {
                // 现在已经满了
                replace();
            }
            // 这个理应是插入到t2中
            t2_.push_front({key, value});
            t2_mp_[key] = t2_.begin();
            repeat_ghost();
#ifndef NDEBUG
            check_key_in_mp2_but_it_error(key, value);
#endif
        } else if (it4 != b2_mp_.end()) {
            // 不在t1,t2 ， 在ghost表 b2 中
            // 这里会存在除 0异常，但是逻辑上已经避免了
            size_t delta = std::max<size_t>(1, b1_.size() / b2_.size());
            p_ = delta > p_ ? 0 : p_ - delta;
            b2_.erase(it4->second);
            b2_mp_.erase(key);
            if (t1_.size() + t2_.size() == capacity_) {
                // 现在已经满了
                replace();
            }
            // 这个理应是插入到t2中
            t2_.push_front({key, value});
            t2_mp_[key] = t2_.begin();
            repeat_ghost();
#ifndef NDEBUG
            check_key_in_mp2_but_it_error(key, value);
#endif
        } else {
            // 所有miss
            if (t1_.size() + t2_.size() == capacity_) {
                // 现在已经满了
                replace();
            }
            t1_.push_front({key, value});
            t1_mp_[key] = t1_.begin();
            repeat_ghost();
#ifndef NDEBUG
            check_key_in_mp1_but_it_error(key, value);
#endif
        }
        check_key_in_mp_only_(key);
        return true;
    }

}
#endif //CACHESTUDY_ARCCACHE_H
