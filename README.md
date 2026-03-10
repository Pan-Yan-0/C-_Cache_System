# cacheStudy（KamaCache 学习版）

> 约定：我（助手）**只会修改 Markdown 文档**（例如 `README.md`、`notes/*.md`）。
> 代码文件（例如 `.cpp/.h/CMakeLists.txt`）由你自己编写与修改，我只做讲解、检查点提示和排错协助。

这是一个用 C++ 从 0 练手实现的本地内存缓存库项目，当前已包含三种页面替换策略：

- **LRU**：Least Recently Used（最近最久未使用）
- **LFU**：Least Frequently Used（最不经常使用，带 LRU 作为同频次 tie-break）
- **ARC**：Adaptive Replacement Cache（自适应替换缓存）

项目形态偏“工程化练手”：
- 有一个对外接口 `ICache<K, V>`（契约/协议）
- 每种策略做一个实现类（`LRUCache` / `LFUCache` / `ARCCache`）
- 有通用契约测试 + 策略专项测试
- 用 CMake + CTest 管理构建与测试

---

## 1. 环境要求

- OS：Linux（你当前是 Ubuntu 22.04）
- 编译器：支持 C++23 的 C++ 编译器（如 g++/clang++）
- 构建工具：CMake（本项目的 `CMakeLists.txt` 使用了较新的版本要求）

---

## 2. 快速开始（构建 / 运行）

推荐使用 out-of-source build（不把构建产物污染源代码目录）：

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

运行示例程序（对应 `src/main.cpp`）：

```bash
./cacheStudy
```

> 说明：`main.cpp` 目前更像一个临时 demo / smoke-run。后续如果你想更工程化，可以增加 `examples/` 目录放示例。

---

## 3. 如何运行测试（CTest）

构建完成后，在 `build/` 目录下运行：

```bash
ctest --output-on-failure
```

当前测试可执行目标（由 CMake 生成）：
- `icache_basic_tests`：接口契约测试（对 LRU/LFU/ARC 都跑一遍）
- `lru_tests`：LRU 专项测试
- `lfu_tests`：LFU 专项测试（含并发 smoke test）
- `arc_tests`：ARC 专项测试

你也可以直接运行某个测试可执行文件，例如：

```bash
./lru_tests
./lfu_tests
./arc_tests
./icache_basic_tests
```

---

## 4. 目录结构（你需要重点关注的文件）

- `include/mycache/`
  - `ICache.h`：缓存接口（契约）
  - `LRUCache.h`：LRU 策略实现（模板 + header-only）
  - `LFUCache.h`：LFU 策略实现（模板 + header-only）
  - `ARCCache.h`：ARC 策略实现（模板 + header-only）
  - `mycache.h`：聚合头（推荐用户 include 的入口）

- `src/`
  - `main.cpp`：运行示例
  - `bench_cache.cpp`：基准测试程序（命中率/吞吐对比）
  - `dummy.cpp`：静态库占位源文件（让 `mycache` 成为“真实库目标”，IDE 索引更稳定）

- `tests/`
  - `test_icache_basic.cpp`：通用契约测试
  - `test_lru.cpp`：LRU 专项
  - `test_lfu.cpp`：LFU 专项
  - `test_arc.cpp`：ARC 专项

- `notes/`
  - `learning-log.md`：学习日志（记录我们分步推进的过程与决策）
  - `benchmark-guide.md`：benchmark 代码讲解（结构、指标、扩展方向）

---

## 5. 对外使用方式（推荐 include）

推荐使用聚合头：

```cpp
#include "mycache/mycache.h"
```

这样你就能直接使用：
- `mycache::ICache<K, V>`
- `mycache::LRUCache<K, V>`
- `mycache::LFUCache<K, V>`
- `mycache::ARCCache<K, V>`

---

## 6. ICache 的契约（Contract / 语义约定）

`ICache<K, V>` 是所有缓存策略实现必须遵守的最小接口。

### 6.1 方法列表

- `bool put(const K& key, const V& value)`
  - 写入或覆盖一个 key
- `std::optional<V> get(const K& key)`
  - 命中返回 value
  - 未命中返回 `std::nullopt`
- `bool erase(const K& key)`
  - 删除 key；成功 `true`，不存在 `false`
- `size_t size() const`
  - 当前缓存条目数量
- `size_t capacity() const`
  - 最大容量

### 6.2 统一语义：capacity == 0 表示“禁用缓存”

为了让上层可以用一个开关关闭缓存，而不用改业务代码，本项目约定：

- 当 `capacity()==0`：
  - `put(...)` 永远返回 `false`（不写入）
  - `get(...)` 永远返回 `std::nullopt`（永远 miss）
  - `erase(...)` 永远返回 `false`（不删除）
  - `size()` 永远为 `0`

这个语义会在 `icache_basic_tests` 里被回归测试覆盖。

### 6.3 覆盖写（put 已存在 key）是否算一次“访问”

本项目当前的约定（LRU/LFU 保持一致，便于使用方理解与测试）：

- `put(key, value)` 如果 key 已存在：
  - 会更新 value
  - 并且把它视作一次访问（会影响淘汰策略：LRU 变“最近使用”；LFU 会提升频次）

> 这属于“缓存契约”的重要部分：不同系统可能会有不同约定，但一定要在项目中写死并用测试锁定。

### 6.4 线程安全（当前实现策略）

- 当前 `LRUCache` / `LFUCache` 采用“**单把大锁**”保护公共 API（put/get/erase/size/capacity）。
- 目标是：
  - 并发调用不崩溃、不产生结构损坏
  - 不变量成立（例如 `size <= capacity`）

并发测试目前属于 **smoke test**：只验证不崩溃与基本不变量，不对并发下的确定性淘汰顺序做断言（避免脆弱测试）。

---

## 7. Benchmark（LRU / LFU / ARC 对比）

构建并运行 benchmark：

```bash
cmake --build . --target bench_cache
./bench_cache
```

### 7.1 Workload
- `HotSet`：热点访问（高局部性）
- `ScanHot`：扫描干扰 + 热点混合
- `Random`：弱局部性基线场景

### 7.2 指标口径
- `Hit(avg)`：多轮平均命中率（**只统计 Get**）
- `Hit(min~max)`：多轮命中率范围
- `ThrAvg(op/s)`：多轮平均吞吐（**统计全部操作**）
- `Thr(min~max)`：多轮吞吐范围

### 7.3 运行稳定性
- `warmup_rounds`：预热轮，不计入结果
- `measured_rounds`：正式计时轮，参与统计

详细代码讲解见：`notes/benchmark-guide.md`

---

## 8. 下一步计划（主线建议）

当你把 LRU/LFU/ARC 都稳定以后，可以考虑下面的工程化主线（按优先级）：

1. 分片缓存（sharding）：降低单大锁竞争（性能优化方向）
2. 更系统的 benchmark：容量 sweep、多 seed、CSV 导出
3.（可选）引入 Catch2/GoogleTest（当测试规模明显增长时）

---

## 9. 许可证 / 说明

这是一个学习型项目，用于练习缓存策略、接口设计、测试分层与并发基础。
