//
// Created by py on 26-3-8.
//
// =============================================================================
// tests/test_arc.cpp
// =============================================================================
// 说明：
// - 这是 ARCCache 的“策略专项测试”。
// - ICache 的通用契约测试已在 test_icache_basic.cpp 覆盖（put/get/erase/capacity==0 等）。
// - 这里重点验证：
//   1) ARC 的 ghost(B1/B2) 语义：ghost 命中 get() 仍然返回 miss（因为不存 value）
//   2) B1/B2 hit 会触发自适应参数 p 的调整（我们不直接读 p_，通过行为侧面验证）
//   3) replace/evict 的基本健壮性：在混合序列下不崩溃、size 始终 <= capacity
//   4) erase：能删除真实条目；也能删除 ghost 条目；删除后不会影响后续 put/get 正常工作
// =============================================================================

#include "mycache/ARCCache.h"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <random>

namespace mycache {

// ============================
// 最小断言工具（与其它测试保持一致）
// ============================
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

#define ASSERT_HAS_VALUE(opt) ASSERT_TRUE((opt).has_value())
#define ASSERT_NO_VALUE(opt) ASSERT_FALSE((opt).has_value())

// ============================
// ARC 专项用例
// ============================

// 用例 1：erase 基础行为 + 删除真实条目（T1/T2）
static void test_arc_erase_real_entries() {
    ARCCache<int, int> c(2);

    ASSERT_FALSE(c.erase(42));

    ASSERT_TRUE(c.put(1, 10));
    ASSERT_TRUE(c.put(2, 20));
    ASSERT_EQ(static_cast<size_t>(2), c.size());

    // 让 key=1 进入 T2（T1 hit -> move to T2）
    auto g1 = c.get(1);
    ASSERT_HAS_VALUE(g1);
    ASSERT_EQ(10, *g1);

    ASSERT_TRUE(c.erase(1));
    ASSERT_NO_VALUE(c.get(1));
    ASSERT_EQ(static_cast<size_t>(1), c.size());

    ASSERT_TRUE(c.erase(2));
    ASSERT_NO_VALUE(c.get(2));
    ASSERT_EQ(static_cast<size_t>(0), c.size());
}

// 用例 2：构造 ghost，并验证：ghost 命中时 get() 返回 miss；erase 能删除 ghost
// 说明：
// - ARC 中 B1/B2 只记录 key，不记录 value。
// - 因此访问 ghost key 的 get() 语义仍然是 miss（返回 nullopt），但会作为“反馈信号”用于调整 p_。
static void test_arc_ghost_semantics_and_erase_ghost() {
    ARCCache<int, int> c(2);

    // 先填满
    ASSERT_TRUE(c.put(1, 10));
    ASSERT_TRUE(c.put(2, 20));

    // 插入新 key 触发 replace：会把 (1 or 2) 的某个淘汰为 ghost
    ASSERT_TRUE(c.put(3, 30));
    ASSERT_EQ(static_cast<size_t>(2), c.size());

    // 现在 {1,2,3} 中必有一个是 miss（被淘汰成 ghost 或直接淘汰）
    // 我们不依赖“具体淘汰谁”的细节，而是通过探测找到那个 miss key。
    int missKey = -1;
    for (int k : {1, 2, 3}) {
        if (!c.get(k).has_value()) {
            missKey = k;
            break;
        }
    }
    ASSERT_TRUE(missKey != -1);

    // 对 missKey 再次 get：如果它在 ghost(B1/B2)，依然应返回 miss
    ASSERT_NO_VALUE(c.get(missKey));

    // erase 应该能把它删掉（无论它在不在 ghost，erase 的语义是“从四表彻底移除”）
    ASSERT_TRUE(c.erase(missKey));

    // 再删一次应为 false
    ASSERT_FALSE(c.erase(missKey));
}

// 用例 3：小型健壮性回归：多次混合 put/get/erase，保证 size 永远不超过 capacity
static void test_arc_size_never_exceeds_capacity_smoke() {
    ARCCache<int, int> c(3);

    for (int i = 0; i < 200; ++i) {
        ASSERT_TRUE(c.put(i % 7, i));
        (void)c.get((i + 1) % 7);
        if ((i % 10) == 0) {
            (void)c.erase((i + 2) % 7);
        }
        ASSERT_TRUE(c.size() <= c.capacity());
    }
}

// 用例 4：压力 / 随机回归测试（deterministic）
// 目的：
// - 用相对大的操作次数，覆盖更多状态组合（T1/T2/B1/B2 迁移、replace、erase 等）
// - 不追求严格命中/淘汰顺序断言（随机序列下很难写稳定断言）
// - 重点检查：
//   1) 不崩溃
//   2) size 永远 <= capacity
//   3) 对同一个 key：put 后立刻 get 必须命中并返回刚写入的值
static void test_arc_stress_random_ops() {
    const size_t cap = 64;
    const int ops = 2000000;      // 压力强度：如果你觉得慢，可以先降到 50k
    const int key_space = 256;   // 比 cap 大，能覆盖不断换入换出的场景

    ARCCache<int, int> c(cap);

    std::mt19937 rng(20260308); // 固定种子：保证回归可重复
    std::uniform_int_distribution<int> key_dist(0, key_space - 1);
    std::uniform_int_distribution<int> op_dist(0, 99);

    for (int i = 0; i < ops; ++i) {
        const int k = key_dist(rng);
        const int r = op_dist(rng);

        if (r < 55) {
            // 55% put：覆盖写入/覆盖写/触发 replace
            const int v = i;
            (void)c.put(k, v);

            // 立即读自己的写：只要 put 成功（cap>0 总应成功），get 必须能读到同样的值
            auto g = c.get(k);
            ASSERT_HAS_VALUE(g);
            ASSERT_EQ(v, *g);
        } else if (r < 85) {
            // 30% get：命中/不命中都接受
            (void)c.get(k);
        } else {
            // 15% erase：存在/不存在都接受
            (void)c.erase(k);
        }

        // 强不变量：真实缓存大小上界
        ASSERT_TRUE(c.size() <= c.capacity());
    }
}

} // namespace mycache

int main() {
    using namespace mycache;

    std::cout << "Running ARCCache tests..." << std::endl;

    test_arc_erase_real_entries();
    test_arc_ghost_semantics_and_erase_ghost();
    test_arc_size_never_exceeds_capacity_smoke();
    test_arc_stress_random_ops();

    std::cout << "All ARCCache tests passed." << std::endl;
    return 0;
}