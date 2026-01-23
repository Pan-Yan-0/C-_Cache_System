//
// Created by py on 26-1-22.
//
// ============================ 学习注释（不含实现代码）============================
// 这个文件是：缓存系统的“接口（Interface）”定义位置。
//
// 为什么要有 ICache.h？
// - 你会实现多种缓存策略：LRU / LFU / ARC。
// - 如果每种策略都用不同的函数名/用法，main.cpp 或测试代码就很难切换。
// - 因此我们先约定“缓存应该具备哪些能力”，形成一个统一的协议：
//     put(key, value)  写入/更新
//     get(key)         读取（可能命中，也可能没命中）
//     erase(key)       删除
//     size()           当前缓存条目数量
//     capacity()       最大容量
//
// 你最终要在这里写什么？（后续步骤会一点点来）
// 1) 一个命名空间：mycache
// 2) 一个“模板接口类”：ICache<K, V>
// 3) 上面提到的最小 API（仅声明，不写算法细节）
//
// 注意：
// - 这个文件只负责“声明规则/接口”，不负责具体的 LRU/LFU/ARC 算法。
// - 真正的算法会写在 LRUCache.h（以及未来的 LFUCache.h / ARCCache.h）里。
// ==============================================================================

#ifndef CACHESTUDY_ICACHE_H
#define CACHESTUDY_ICACHE_H
#include <cstddef>
#include <optional>

namespace mycache {
    // 这里之后会放：ICache<K, V> 的声明（接口）。
    // 现在先留空，等你准备好了我们再一步一步补。
    template<class K, class V>
    class ICache {
    public:
        // 写入
        virtual bool put(const K &key, const V &value) = 0;
        // 读取
        //V get(const K &key);
        virtual std::optional<V> get(const K &key) = 0;
        // 擦除
        virtual bool erase(const K &key) = 0;
        // 当前存储个数
        virtual size_t size() const = 0;
        // 最大容量
        virtual size_t capacity() const = 0;

    public:
        // 防止无法析构成功
        virtual ~ICache() = default;
    };
}

#endif //CACHESTUDY_ICACHE_H
