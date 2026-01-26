//
// Created by py on 26-1-26.
//
// =============================================================================
// tests/test_lfu.cpp
// =============================================================================
// 说明：
// 1) 这是 LFUCache 的“策略专项测试”。
//    - ICache 的通用语义（put/get/erase/capacity==0 等）已经在 test_icache_basic.cpp 覆盖。
//    - 这里重点验证：LFU 的淘汰规则 + 同频次下的 LRU tie-break。
//
// 2) 测试风格：AAA（Arrange / Act / Assert）
//    - Arrange：准备缓存和初始数据
//    - Act：执行 put/get/erase 等操作
//    - Assert：断言返回值/是否淘汰/size 等
//
// 3) 并发测试说明：
//    - 并发 smoke test 只验证“不会崩溃、不触发不变量、size<=capacity”。
//    - 不验证并发下的确定淘汰顺序（因为线程调度不可控，做确定性断言会很脆）。
// =============================================================================

#include "mycache/LFUCache.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <random>
#include <condition_variable>
#include <mutex>

namespace mycache {

// ============================
// 最小断言工具（不依赖第三方库）
// ============================
// 约定：断言失败 -> 打印失败位置与信息 -> exit(1)
// 好处：
// - 不需要引入测试框架（Catch2/GoogleTest）也能做最小回归验证
// - 任意测试失败会让可执行程序返回非 0，方便 CTest 判断失败
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
// LFU 专项测试
// ============================

    // 基础 put/get：
    // - get miss
    // - put 后 get hit
    // - size/capacity 行为
    void test_put_get_basic() {
        // Arrange
        LFUCache<int, int> c(2);

        // Act + Assert
        ASSERT_NO_VALUE(c.get(1));

        ASSERT_TRUE(c.put(1, 100));
        auto v = c.get(1);
        ASSERT_HAS_VALUE(v);
        ASSERT_EQ(100, *v);

        ASSERT_EQ(static_cast<size_t>(1), c.size());
        ASSERT_EQ(static_cast<size_t>(2), c.capacity());
    }

    // capacity==0 语义：禁用缓存
    // - put 返回 false
    // - get 返回 nullopt
    // - erase 返回 false
    // - size 恒为 0
    void test_capacity_zero_semantics() {
        // Arrange
        LFUCache<int, int> c(0);

        // Assert
        ASSERT_EQ(static_cast<size_t>(0), c.size());
        ASSERT_EQ(static_cast<size_t>(0), c.capacity());

        ASSERT_FALSE(c.put(1, 1));
        ASSERT_NO_VALUE(c.get(1));
        ASSERT_FALSE(c.erase(1));

        ASSERT_EQ(static_cast<size_t>(0), c.size());
    }

    // LFU 淘汰（按频次）：
    // cap=2:
    //   put(1), put(2) -> 两者 freq=1
    //   get(1)         -> 1 的 freq 变成 2
    //   put(3)         -> 由于容量满，应淘汰 freq 最小的 2
    void test_lfu_eviction_by_freq() {
        // Arrange
        LFUCache<int, int> c(2);

        // Act
        ASSERT_TRUE(c.put(1, 10)); // f1=1
        ASSERT_TRUE(c.put(2, 20)); // f2=1
        ASSERT_HAS_VALUE(c.get(1)); // f1=2

        // Assert（插入新 key 触发淘汰）
        ASSERT_TRUE(c.put(3, 30));  // 应淘汰 key=2

        ASSERT_NO_VALUE(c.get(2));
        ASSERT_HAS_VALUE(c.get(1));
        ASSERT_HAS_VALUE(c.get(3));
    }

    // 同频次 tie-break：在同一个 freq 桶内按 LRU 淘汰
    // cap=2:
    //   put(1) -> freq=1 bucket: [1]
    //   put(2) -> freq=1 bucket: [2,1]   (front=MRU)
    //   put(3) -> 淘汰 bucket.back() -> 1
    void test_lfu_tie_breaker_is_lru_within_bucket() {
        // Arrange
        LFUCache<int, int> c(2);

        // Act
        c.put(1, 10);
        c.put(2, 20);
        c.put(3, 30);

        // Assert
        ASSERT_NO_VALUE(c.get(1));
        ASSERT_HAS_VALUE(c.get(2));
        ASSERT_HAS_VALUE(c.get(3));
    }

    // 覆盖写（put 已存在 key）是否：
    // 1) 更新 value
    // 2) 算作一次访问（freq++）
    // 该语义会影响淘汰结果（这是 LFU 项目里经常需要明确的“契约点”）
    void test_overwrite_put_increases_freq_and_updates_value() {
        // Arrange
        LFUCache<int, int> c(2);
        c.put(1, 10); // f1=1
        c.put(2, 20); // f2=1

        // Act
        ASSERT_TRUE(c.put(1, 100)); // 更新值 + touch: f1=2
        auto v1 = c.get(1);         // 再 touch: f1=3

        // Assert
        ASSERT_HAS_VALUE(v1);
        ASSERT_EQ(100, *v1);

        c.put(3, 30);               // capacity 满：应淘汰 2（f2=1 最小）

        ASSERT_NO_VALUE(c.get(2));
        ASSERT_HAS_VALUE(c.get(1));
        ASSERT_HAS_VALUE(c.get(3));
    }

    // erase 基础：存在/不存在的删除、删除后 get miss、size 变化
    void test_erase_basic() {
        // Arrange
        LFUCache<std::string, int> c(2);

        // Act + Assert
        ASSERT_FALSE(c.erase("nope"));

        c.put("a", 1);
        c.put("b", 2);

        ASSERT_TRUE(c.erase("a"));
        ASSERT_EQ(static_cast<size_t>(1), c.size());
        ASSERT_NO_VALUE(c.get("a"));
        ASSERT_HAS_VALUE(c.get("b"));

        ASSERT_TRUE(c.erase("b"));
        ASSERT_EQ(static_cast<size_t>(0), c.size());
        ASSERT_NO_VALUE(c.get("b"));
    }

    // erase 对 minFreq_ 的影响：
    // - 很多 LFU bug 都出在“删除导致 minFreq_ 指向不存在桶”，再 put 触发淘汰时崩。
    // - 这个用例做一个简单回归：erase 后继续 put 不应异常。
    void test_erase_does_not_break_future_put() {
        // Arrange
        LFUCache<int, int> c(2);
        c.put(1, 10);
        c.put(2, 20);

        // Act
        ASSERT_TRUE(c.erase(1));
        ASSERT_TRUE(c.put(3, 30));

        // Assert
        ASSERT_NO_VALUE(c.get(1));
        ASSERT_HAS_VALUE(c.get(2));
        ASSERT_HAS_VALUE(c.get(3));
        ASSERT_EQ(static_cast<size_t>(2), c.size());
    }

    // 并发 smoke：验证线程安全（更准确说：验证“单锁实现不会在并发下崩溃”）
    // 关键点：
    // - 用 condition_variable 做一个“起跑线”，让多个线程尽可能同时开始，增加交错概率。
    // - 最后只断言 size<=capacity（并发下最终命中/淘汰的确定性结果不可断言）。
    void test_concurrent_smoke() {
        const int thread_count = 8;
        const int ops_per_thread = 500000;
        const int key_space = 64;

        // Arrange
        LFUCache<int, int> c(32);

        std::mutex m;
        std::condition_variable cv_ready;
        std::condition_variable cv_start;
        int ready_count = 0;
        bool start = false;

        // 每个线程里用独立 RNG，避免共享随机数引擎引入额外数据竞争
        auto worker = [&](int tid) {
            std::mt19937 rng(static_cast<unsigned>(tid + 20260126));
            std::uniform_int_distribution<int> key_dist(0, key_space - 1);
            std::uniform_int_distribution<int> op_dist(0, 2); // 0/1/2 -> put/get/erase

            // 起跑线：等待主线程发令
            {
                std::unique_lock<std::mutex> lk(m);
                ++ready_count;
                cv_ready.notify_one();
                cv_start.wait(lk, [&] { return start; });
            }

            // Act：随机操作同一个 cache
            for (int i = 0; i < ops_per_thread; ++i) {
                int k = key_dist(rng);
                int op = op_dist(rng);
                if (op == 0) {
                    c.put(k, k + i);
                } else if (op == 1) {
                    (void)c.get(k);
                } else {
                    (void)c.erase(k);
                }
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        for (int t = 0; t < thread_count; ++t) threads.emplace_back(worker, t);

        // 等所有 worker 都 ready，再统一 start
        {
            std::unique_lock<std::mutex> lk(m);
            cv_ready.wait(lk, [&] { return ready_count == thread_count; });
            start = true;
        }
        cv_start.notify_all();

        for (auto &th : threads) th.join();

        // Assert：不变量最基本的一条（缓存大小永远不能超过容量）
        ASSERT_TRUE(c.size() <= c.capacity());
    }

} // namespace mycache

int main() {
    using namespace mycache;

    std::cout << "Running LFUCache tests..." << std::endl;

    test_put_get_basic();
    test_capacity_zero_semantics();
    test_lfu_eviction_by_freq();
    test_lfu_tie_breaker_is_lru_within_bucket();
    test_overwrite_put_increases_freq_and_updates_value();
    test_erase_basic();
    test_erase_does_not_break_future_put();
    test_concurrent_smoke();

    std::cout << "All LFU tests passed." << std::endl;
    return 0;
}
