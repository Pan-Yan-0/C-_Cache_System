//
// Created by py on 26-1-23.
//
#include "mycache/LRUCache.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace mycache {

// ============================
// 最小断言工具（不依赖第三方库）
// ============================
// 约定：断言失败就打印位置 + 期望/实际，并直接退出进程（让 CTest 判定失败）。

#define MYCACHE_FAIL(msg)                                                                    \
    do {                                                                                     \
        std::cerr << "[TEST FAILED] " << __FILE__ << ":" << __LINE__ << " - " << msg      \
                  << std::endl;                                                              \
        std::exit(1);                                                                        \
    } while (0)

#define ASSERT_TRUE(expr)                                                                    \
    do {                                                                                     \
        if (!(expr)) {                                                                       \
            MYCACHE_FAIL(std::string("ASSERT_TRUE failed: ") + #expr);                      \
        }                                                                                    \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define ASSERT_EQ(expected, actual)                                                          \
    do {                                                                                     \
        const auto &_exp = (expected);                                                       \
        const auto &_act = (actual);                                                         \
        if (!(_exp == _act)) {                                                               \
            std::cerr << "[TEST FAILED] " << __FILE__ << ":" << __LINE__                  \
                      << " - ASSERT_EQ failed\n  expected: " << _exp                        \
                      << "\n  actual:   " << _act << std::endl;                              \
            std::exit(1);                                                                    \
        }                                                                                    \
    } while (0)

// 用于 optional 的断言：命中/不命中
#define ASSERT_HAS_VALUE(opt) ASSERT_TRUE((opt).has_value())
#define ASSERT_NO_VALUE(opt) ASSERT_FALSE((opt).has_value())

// ============================
// 测试用例
// ============================

    void test_put_get_basic() {
        LRUCache<int, int> c(2);

        // 初始：get miss
        auto v0 = c.get(1);
        ASSERT_NO_VALUE(v0);

        // put 后：get hit
        ASSERT_TRUE(c.put(1, 100));
        auto v1 = c.get(1);
        ASSERT_HAS_VALUE(v1);
        ASSERT_EQ(100, *v1);

        // size/capacity 基本行为
        ASSERT_EQ(static_cast<size_t>(1), c.size());
        ASSERT_EQ(static_cast<size_t>(2), c.capacity());
    }

    void test_lru_eviction_order() {
        // capacity=2：放入 1,2；访问 1；再放 3 -> 应淘汰 2
        LRUCache<int, int> c(2);

        c.put(1, 10);
        c.put(2, 20);

        auto hit1 = c.get(1);
        ASSERT_HAS_VALUE(hit1);
        ASSERT_EQ(10, *hit1);

        c.put(3, 30);

        auto miss2 = c.get(2);
        auto hit1_again = c.get(1);
        auto hit3 = c.get(3);

        ASSERT_NO_VALUE(miss2);
        ASSERT_HAS_VALUE(hit1_again);
        ASSERT_EQ(10, *hit1_again);

        ASSERT_HAS_VALUE(hit3);
        ASSERT_EQ(30, *hit3);
    }

    void test_erase() {
        LRUCache<std::string, int> c(2);

        // 空缓存 erase
        ASSERT_FALSE(c.erase("nope"));

        c.put("a", 1);
        c.put("b", 2);

        ASSERT_FALSE(c.erase("C"));

        ASSERT_TRUE(c.erase("a"));
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        auto a = c.get("a");
        ASSERT_NO_VALUE(a);

        auto b = c.get("b");
        ASSERT_HAS_VALUE(b);
        ASSERT_EQ(2, *b);
    }

    void test_put_same_key_does_not_grow_size() {
        LRUCache<int, int> c(2);

        c.put(1, 100);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        // 重复 put 同一个 key：正确行为是更新 value，但 size 不应增加
        c.put(1, 200);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        auto v = c.get(1);
        ASSERT_HAS_VALUE(v);
        ASSERT_EQ(200, *v);
    }

    void test_volumn_by_one_lru() {
        // 测试capacity为 1 下 lru 的工作情况
        LRUCache<int, int> lruCache(1);
        lruCache.put(1, 1);
        ASSERT_EQ(static_cast<size_t>(1), lruCache.size());

        // 重复放入测试
        lruCache.put(1, 100);
        ASSERT_EQ(1, lruCache.size());

        auto g = lruCache.get(1);
        ASSERT_HAS_VALUE(g);
        // ASSERT_HAS_VALUE(g.has_value());
        ASSERT_EQ(100, *g);

        lruCache.put(2, 200);
        ASSERT_EQ(1, lruCache.size());
        g = lruCache.get(2);
        ASSERT_HAS_VALUE(g);
        ASSERT_EQ(200, *g);
        auto g1 = lruCache.get(1);
        ASSERT_NO_VALUE(g1);

    }

    void test_use_get_many_times_look_the_size() {
        LRUCache<int, int> c(2);
        c.put(1, 1);
        auto c_key1 = c.get(1);
        ASSERT_HAS_VALUE(c_key1);
        ASSERT_EQ(1, *c_key1);
        ASSERT_EQ(1, c.size());
        c_key1 = c.get(1);
        ASSERT_HAS_VALUE(c_key1);
        ASSERT_EQ(1, *c_key1);
        ASSERT_EQ(1, c.size());
    }

    void test_cover_write_and_value_or_recent_use() {
        LRUCache<int, int> c(2);
        c.put(1, 1);
        c.put(2, 2);

        c.put(1, 100);
        c.put(3, 1);
        auto c_key1 = c.get(1);
        ASSERT_HAS_VALUE(c_key1);
        ASSERT_EQ(100, *c_key1);

        auto c_key2 = c.get(2);
        ASSERT_NO_VALUE(c_key2);

        ASSERT_EQ(static_cast<size_t>(2), c.size());

        auto c_key3 = c.get(3);
        ASSERT_HAS_VALUE(c_key3);
    }

    void test_erase_put() {
        LRUCache<int, int> c(2);
        c.put(1, 1);
        c.put(2, 2);

        c.erase(1);
        auto c_key1 = c.get(1);
        ASSERT_NO_VALUE(c_key1);
        ASSERT_EQ(1, c.size());
        c.put(3, 3);
        c.put(4, 4);
        auto c_key2 = c.get(2);
        ASSERT_NO_VALUE(c_key2);

        auto c_key3 = c.get(3);
        ASSERT_HAS_VALUE(c_key3);

        auto c_key4 = c.get(4);
        ASSERT_HAS_VALUE(c_key4);
    }

    void test_capacity_zero() {
        LRUCache<int, int> c(0);
        ASSERT_EQ(0,c.size());
        ASSERT_EQ(0,c.capacity());
        bool put = c.put(1, 1);
        ASSERT_EQ(false, put);
        auto c_key1 = c.get(1);
        ASSERT_NO_VALUE(c_key1);
        bool erase = c.erase(1);
        ASSERT_EQ(false, erase);
        ASSERT_EQ(0,c.size());
        ASSERT_EQ(0,c.capacity());
    }

} // namespace mycache

int main() {
    using namespace mycache;

    std::cout << "Running LRUCache tests..." << std::endl;

    test_put_get_basic();
    test_lru_eviction_order();
    test_erase();
    test_put_same_key_does_not_grow_size();
    test_volumn_by_one_lru();
    test_use_get_many_times_look_the_size();
    test_cover_write_and_value_or_recent_use();
    test_erase_put();
    test_capacity_zero();
    std::cout << "All tests passed." << std::endl;
    return 0;
}
