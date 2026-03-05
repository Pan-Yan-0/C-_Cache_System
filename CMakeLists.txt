cmake_minimum_required(VERSION 3.28)
project(cacheStudy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 缓存库（先用一个占位源文件，让它成为“真实”库目标，IDE 更容易索引）
add_library(mycache STATIC
        src/dummy.cpp
        include/mycache/mycache.h
        include/mycache/ICache.h
        include/mycache/LRUCache.h
        include/mycache/LFUCache.h
        include/mycache/ARCCache.h
)

# PUBLIC：mycache 自己编译需要；链接到 mycache 的目标也会继承该 include 路径
target_include_directories(mycache PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

add_executable(cacheStudy
        src/main.cpp
)

# main 依赖缓存库
target_link_libraries(cacheStudy PRIVATE mycache)

# ============================
# Tests (CTest)
# ============================
enable_testing()

add_executable(lru_tests
        tests/test_lru.cpp
)

target_link_libraries(lru_tests PRIVATE mycache)

add_test(NAME lru_tests COMMAND lru_tests)

add_executable(lfu_tests
        tests/test_lfu.cpp
)

target_link_libraries(lfu_tests PRIVATE mycache)

add_test(NAME lfu_tests COMMAND lfu_tests)

add_executable(icache_basic_tests
        tests/test_icache_basic.cpp
)

target_link_libraries(icache_basic_tests PRIVATE mycache)

add_test(NAME icache_basic_tests COMMAND icache_basic_tests)