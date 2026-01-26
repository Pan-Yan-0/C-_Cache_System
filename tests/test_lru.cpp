//
// Created by py on 26-1-23.
//
// =============================================================================
// tests/test_lru.cpp
// =============================================================================
// 说明：
// 1) 这是 LRUCache 的“策略专项测试”。
//    - ICache 的通用语义（put/get/erase/capacity==0 等）已在 test_icache_basic.cpp 覆盖。
//    - 这里重点验证：LRU 的淘汰顺序、覆盖写是否影响“最近使用”、以及 erase 相关边界。
//
// 2) 测试风格：AAA（Arrange / Act / Assert）
//    - Arrange：准备缓存和初始数据
//    - Act：执行 put/get/erase
//    - Assert：断言命中/淘汰/size 等
//
// 3) 并发测试说明：
//    - 并发 smoke test 只验证“不崩溃 + size<=capacity”。
//    - 不验证并发下严格淘汰顺序/返回值一致性（线程调度不可控，易产生脆弱测试）。
// =============================================================================

#include "mycache/LRUCache.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <random>
#include <condition_variable>

// 注意：本文件在并发 smoke test 中会直接使用 std::thread（见 test_concurrent_smoke 的 std::vector<std::thread>）。
// 某些 clangd/静态分析会在解析不完整或索引未更新时误报 unused include；保持此 include 更稳妥。

namespace mycache {

// ============================
// 最小断言工具（不依赖第三方库）
// ============================
// 约定：断言失败 -> 打印失败位置与信息 -> exit(1)
// 好处：
// - 不引入第三方测试框架，也能完成回归验证
// - 失败时能快速定位到哪一行
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
        // Arrange
        LRUCache<int, int> c(2);

        // Act + Assert
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
        // Arrange
        // capacity=2：放入 1,2；访问 1；再放 3 -> 应淘汰 2
        LRUCache<int, int> c(2);

        // Act
        c.put(1, 10);
        c.put(2, 20);

        auto hit1 = c.get(1); // 1 变为 MRU
        ASSERT_HAS_VALUE(hit1);
        ASSERT_EQ(10, *hit1);

        c.put(3, 30); // 触发淘汰

        // Assert
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
        // Arrange
        LRUCache<std::string, int> c(2);

        // Act + Assert
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
        // Arrange
        LRUCache<int, int> c(2);

        // Act
        c.put(1, 100);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        // 重复 put 同一个 key：正确行为是更新 value，但 size 不应增加
        c.put(1, 200);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        // Assert
        auto v = c.get(1);
        ASSERT_HAS_VALUE(v);
        ASSERT_EQ(200, *v);
    }

    void test_volumn_by_one_lru() {
        // capacity=1 的边界：任何新 key 插入都会淘汰旧 key
        LRUCache<int, int> lruCache(1);
        lruCache.put(1, 1);
        ASSERT_EQ(static_cast<size_t>(1), lruCache.size());

        // 覆盖写同 key，不应该改变 size
        lruCache.put(1, 100);
        ASSERT_EQ(static_cast<size_t>(1), lruCache.size());

        auto g = lruCache.get(1);
        ASSERT_HAS_VALUE(g);
        ASSERT_EQ(100, *g);

        // 插入新 key，旧 key 被淘汰
        lruCache.put(2, 200);
        ASSERT_EQ(static_cast<size_t>(1), lruCache.size());
        g = lruCache.get(2);
        ASSERT_HAS_VALUE(g);
        ASSERT_EQ(200, *g);
        auto g1 = lruCache.get(1);
        ASSERT_NO_VALUE(g1);

    }

    void test_use_get_many_times_look_the_size() {
        // 重复 get 不应改变 size
        LRUCache<int, int> c(2);
        c.put(1, 1);
        auto c_key1 = c.get(1);
        ASSERT_HAS_VALUE(c_key1);
        ASSERT_EQ(1, *c_key1);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        c_key1 = c.get(1);
        ASSERT_HAS_VALUE(c_key1);
        ASSERT_EQ(1, *c_key1);
        ASSERT_EQ(static_cast<size_t>(1), c.size());
    }

    void test_cover_write_and_value_or_recent_use() {
        // 覆盖写会更新 value，同时也应视作“最近使用”（避免被当作 LRU 淘汰）
        LRUCache<int, int> c(2);
        c.put(1, 1);
        c.put(2, 2);

        c.put(1, 100); // 覆盖写：1 变为 MRU
        c.put(3, 1);   // 淘汰应发生在 2

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
        // erase 后再 put：不应出现 iterator/map 不一致问题
        LRUCache<int, int> c(2);
        c.put(1, 1);
        c.put(2, 2);

        c.erase(1);
        auto c_key1 = c.get(1);
        ASSERT_NO_VALUE(c_key1);
        ASSERT_EQ(static_cast<size_t>(1), c.size());

        c.put(3, 3);
        c.put(4, 4);

        // 最终应淘汰 2（取决于实现细节：这里的断言用于回归验证你当前语义）
        auto c_key2 = c.get(2);
        ASSERT_NO_VALUE(c_key2);

        auto c_key3 = c.get(3);
        ASSERT_HAS_VALUE(c_key3);

        auto c_key4 = c.get(4);
        ASSERT_HAS_VALUE(c_key4);
    }

    void test_capacity_zero() {
        // capacity==0：禁用缓存语义
        LRUCache<int, int> c(0);
        ASSERT_EQ(static_cast<size_t>(0), c.size());
        ASSERT_EQ(static_cast<size_t>(0), c.capacity());
        bool put = c.put(1, 1);
        ASSERT_EQ(false, put);
        auto c_key1 = c.get(1);
        ASSERT_NO_VALUE(c_key1);
        bool erase = c.erase(1);
        ASSERT_EQ(false, erase);
        ASSERT_EQ(static_cast<size_t>(0), c.size());
        ASSERT_EQ(static_cast<size_t>(0), c.capacity());
    }

    void test_concurrent_smoke() {

        // 并发 smoke：验证“单锁实现”在多线程下不崩溃。
        // - 使用 condition_variable 做起跑线，增加交错概率。
        // - 最后断言 size<=capacity（并发下最终结果不做确定性断言）。

        const int thread_count = 8;
        const int ops_per_thread = 2000000;
        const int key_space = 30;

        LRUCache<int, int> c(32);

        std::mutex m;
        std::condition_variable cv_ready;
        std::condition_variable cv_start;

        int ready_count = 0;        // 到“起跑线”的 worker 数
        bool start = false;         // 主线程发令开跑


        auto worker = [&](int tid) -> void {
            std::mt19937 rng(static_cast<unsigned >(tid + 20260124));
            std::uniform_int_distribution<int> key_dist(0, key_space - 1);
            std::uniform_int_distribution<int> op_dist(0, 2);

            // 起跑线
            {
                std::unique_lock<std::mutex> lk(m);
                ++ready_count;
                cv_ready.notify_one();

                cv_start.wait(lk, [&] { return start; });
            }
            for (int i = 0; i < ops_per_thread; ++i) {
                int k = key_dist(rng);
                int op = op_dist(rng);

                if (op == 0) {
                    c.put(k, k + i); // value 随意
                } else if (op == 1) {
                    (void) c.get(k);
                } else {
                    (void) c.erase(k);
                }
            }


        };
        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back(worker, t);
        }

        {
            std::unique_lock<std::mutex> lk(m);
            cv_ready.wait(lk, [&] { return ready_count == thread_count; });
            start = true;
        }
        cv_start.notify_all();

        for (auto &th: threads) th.join();

        ASSERT_TRUE(c.size() <= c.capacity());
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
    test_concurrent_smoke();
    std::cout << "All tests passed." << std::endl;
    return 0;
}
