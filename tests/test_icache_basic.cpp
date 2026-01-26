//
// Created by py on 26-1-26.
//
// =============================================================================
// tests/test_icache_basic.cpp
// =============================================================================
// 这是“接口级别”的通用测试，用来验证 ICache<K,V> 的基础契约（Contract）。
//
// 为什么要有这一层？
// - LRU/LFU/ARC 等策略都实现同一套 ICache 接口。
// - 通用语义（例如：capacity==0、重复 put 不增长 size、erase 行为等）应该只写一份测试。
// - 策略专项行为（LRU 淘汰顺序 / LFU 频次淘汰）则放到各自的 test_lru.cpp / test_lfu.cpp。
//
// 测试风格：AAA（Arrange / Act / Assert）
// =============================================================================

#include "mycache/LFUCache.h"
#include "mycache/ICache.h"
#include "mycache/LRUCache.h"


#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace mycache {

// ============================
// 最小断言工具（不依赖第三方库）
// ============================
// 约定：断言失败 -> 打印位置 -> exit(1)
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

    template<class CacheT>
    std::unique_ptr<ICache<int, int>> make_cache_int_int(size_t cap) {
        return std::make_unique<CacheT>(cap);
    }

    // run_icache_basic_suite：对任意 ICache 实现跑一遍“基础契约”测试。
    // 只要是实现了 ICache 接口的缓存，都应该通过这里的用例。
    template<class MakeFn>
    void run_icache_basic_suite(const MakeFn &make) {
        {
            // ----------------------------
            // 用例 1：capacity==0 禁用缓存
            // ----------------------------
            // Arrange
            auto c = make(0);
            // Assert
            ASSERT_EQ(static_cast<size_t>(0), c->capacity());
            ASSERT_EQ(static_cast<size_t>(0), c->size());

            // Act + Assert
            ASSERT_FALSE(c->put(1, 1));
            ASSERT_NO_VALUE(c->get(1));
            ASSERT_FALSE(c->erase(1));
            ASSERT_EQ(static_cast<size_t>(0), c->capacity());
            ASSERT_EQ(static_cast<size_t>(0), c->size());


        }
        {
            // ----------------------------
            // 用例 2：基本 put/get 命中
            // ----------------------------
            // Arrange
            auto c2 = make(2);

            // Act
            ASSERT_TRUE(c2->put(1, 1));
            ASSERT_TRUE(c2->put(2, 2));

            auto v1 = c2->get(1);
            auto v2 = c2->get(2);

            // Assert
            ASSERT_HAS_VALUE(v1);
            ASSERT_HAS_VALUE(v2);

            ASSERT_EQ(1, *v1);

            ASSERT_EQ(static_cast<size_t>(2), c2->size());
            ASSERT_EQ(static_cast<size_t>(2), c2->capacity());

        }
        {
            // ----------------------------
            // 用例 3：erase 行为
            // ----------------------------
            // Arrange
            auto c = make(2);

            // Act + Assert
            ASSERT_FALSE(c->erase(42));
            ASSERT_TRUE(c->put(1, 1));
            ASSERT_TRUE(c->erase(1));
            ASSERT_NO_VALUE(c->get(1));
            ASSERT_EQ(static_cast<size_t>(0), c->size());

        }
        {
            // ----------------------------
            // 用例 4：重复 put（覆盖写）
            // ----------------------------
            // Arrange
            auto c = make(2);

            // Act
            ASSERT_TRUE(c->put(1, 100));
            ASSERT_EQ(static_cast<size_t>(1), c->size());
            ASSERT_TRUE(c->put(1, 200)); // 覆盖写

            // Assert
            ASSERT_EQ(static_cast<size_t>(1), c->size()); // size 不增长

            auto g = c->get(1);
            ASSERT_HAS_VALUE(g);
            ASSERT_EQ(static_cast<int>(200), *g);

        }
    }
}// namespace mycache
int main() {
    using namespace mycache;

    std::cout << "Running ICache basic suite on LRU and LFU..." << std::endl;

    // 这里体现“测试分层”：
    // - 先跑通用契约测试（两种策略都必须通过）
    // - 再跑策略专项测试（见 test_lru.cpp / test_lfu.cpp）

    run_icache_basic_suite([](size_t cap) {
        return make_cache_int_int<LRUCache<int, int>>(cap);
    });

    run_icache_basic_suite([](size_t cap) {
        return make_cache_int_int<LFUCache<int, int>>(cap);
    });

    std::cout << "All ICache basic tests passed." << std::endl;
    return 0;
}