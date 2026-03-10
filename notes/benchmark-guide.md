# benchmark-guide：KamaCache 基准测试讲解

本文专门解释 `src/bench_cache.cpp` 的设计与实现，目标是让你能看懂、改得动、并且知道每个模块为什么这样写。

## 1. 这个 benchmark 想回答什么问题？

核心问题：在同样的访问序列下，`LRU / LFU / ARC` 的表现差异是什么？

我们关心两个指标：
- 命中率（只统计 `Get`）
- 吞吐量（统计全部操作）

为什么这样定义？
- `Put/Erase` 不应该混进命中率分母，否则“命中率”语义会失真。
- 吞吐量用总操作数更直观，能反映整体处理速度。

## 2. 代码整体结构（先看全图）

`bench_cache.cpp` 可以分为四层：

1) **Workload 生成层**
- `createHotSetTrace(...)`
- `createScanHotTrace(...)`
- `createRandomTrace(...)`

2) **单次执行层**
- `runBenchmark(...)`

3) **多轮汇总层**
- `summarizeResults(...)`
- `runBenchmarkCase(...)`

4) **输出展示层**
- `printSummaryHeader()`
- `printSummaryRow(...)`

这种分层的好处：
- 你以后想换 workload，不用改统计逻辑。
- 你以后想导出 CSV/JSON，不用改执行逻辑。

## 3. Workload 为什么分这三种？

### 3.1 HotSet

特点：
- 大部分访问集中在小热点区（高局部性）

用途：
- 测试策略在“热点明显”场景的表现。

典型现象：
- LRU/ARC 往往表现好。

### 3.2 ScanHot

特点：
- 扫描流 + 间歇热点访问

用途：
- 测试策略抗扫描污染能力（scan pollution）。

典型现象：
- ARC 常常优于纯 LRU。

### 3.3 Random

特点：
- key 与操作都随机，局部性弱

用途：
- 作为“基线场景”，观察没有明显模式时的表现。

典型现象：
- 三种策略命中率通常更接近。

## 4. 单次执行：`runBenchmark(...)`

### 4.1 它做了什么

- 遍历 trace
- 对每个 entry 调 `cache.get/put/erase`
- 统计：
  - `get_ops/put_ops/erase_ops`
  - `hits/misses`
- 计时：
  - `std::chrono::steady_clock`

### 4.2 为什么用 `steady_clock`

因为 benchmark 更关心“时间间隔”，`steady_clock` 适合做耗时测量，受系统时间调整影响更小。

## 5. 为什么要做 warmup + 多轮？

### 5.1 warmup 的作用

第一轮通常会受一些冷启动因素影响（例如分配器状态、分支预测刚建立）。

所以先跑 `warmup_rounds`，不计入最终统计，更接近稳定状态。

### 5.2 多轮 measured rounds 的作用

单次结果容易有波动。多轮后可以得到：
- 平均值（avg）
- 最小值（min）
- 最大值（max）

这比只看一轮结果更可靠。

## 6. 汇总结构：`BenchmarkSummary`

这个结构保存的是“多轮之后”的结果：
- 平均命中率
- 平均耗时
- 平均吞吐
- 对应 min/max 区间

你看到的 `Hit(min~max)`、`Thr(min~max)` 就来自这里。

## 7. 输出层为什么单独拆函数？

`printSummaryHeader()` 和 `printSummaryRow(...)` 独立存在，主要为了：
- 保持 `main()` 简洁
- 未来切换输出格式更容易

例如你想导出 CSV，可以新增 `writeSummaryCsv(...)`，而不影响 benchmark 核心逻辑。

## 8. 代码里用到的 C++ 特性（你可以重点学）

### 8.1 `std::optional`

在缓存接口里用于表示命中/未命中：
- 有值：命中
- `std::nullopt`：未命中

### 8.2 `[[nodiscard]]`

用于提醒调用者不要忽略重要返回值（比如命中率计算函数）。

### 8.3 `std::chrono`

用于做高精度耗时统计。

### 8.4 结构化分层 + 小函数组合

这不是语法新特性，但是真正的工程能力：
- 小函数职责单一
- 主流程（`main`）可读
- 易测试、易扩展

## 9. 如何运行 benchmark

在构建目录运行（路径按你的工程调整）：

```bash
cd /home/py/CLionProjects/cacheStudy/cmake-build-debug
cmake --build . --target bench_cache
./bench_cache
```

## 10. 如何继续扩展（下一步建议）

1. 容量 sweep：`capacity = 64/128/256/...`
2. 多 seed 重复实验，汇总跨 seed 的平均结果
3. 导出 CSV，方便用 Python/Excel 画图
4. 增加 Zipf workload（更贴近真实热点分布）

---

一句话总结：
> 这份 benchmark 的价值不只是“跑一个表”，而是建立了一个可复现、可解释、可扩展的比较框架。

## 11. 常见误区与排错清单

这一节专门帮你在 benchmark 结果“看起来怪怪的”时快速定位问题。

### 11.1 常见误区

1. **把命中率的分母算成总操作数**
   - 正确口径：命中率只统计 `Get`。
   - `Put/Erase` 只影响缓存状态，不应进入命中率分母。

2. **拿单轮结果直接下结论**
   - 单轮很容易受噪声影响（调度、缓存冷热、分配器状态）。
   - 应优先看 `avg`，再看 `min~max` 波动范围。

3. **忽略 warmup 的作用**
   - 没有预热时，首轮通常偏离稳定状态。
   - 建议保留 `warmup_rounds >= 1`。

4. **不同策略没有使用同一条 trace**
   - 公平比较要求：同一 workload、同一 seed、同一 trace 喂给不同策略。
   - 否则结果不可比。

5. **把吞吐量差异直接等同于“算法优劣”**
   - 吞吐受实现细节影响很大（数据结构、锁粒度、分支路径、分配器行为）。
   - 需要同时看命中率和吞吐，按场景做权衡。

6. **误把 Random 场景当成策略优势场景**
   - Random 的价值是“基线对照”，不是放大策略差异。
   - 三者接近通常是正常现象。

7. **只看一个容量就做结论**
   - 不同容量可能改变策略排序。
   - 更稳妥做法是做 capacity sweep（例如 64/128/256/...）。

---

### 11.2 快速排错清单（建议按顺序）

1. **先确认配置是否一致**
   - `capacity / key_space / ops / read_ratio / rounds / warmup_rounds`
   - 不一致时先不要比较结果。

2. **确认 workload 是否按预期生成**
   - `HotSet`：是否真的有热点集中。
   - `ScanHot`：是否存在扫描+热点混合。
   - `Random`：是否接近均匀随机。

3. **确认统计口径**
   - 命中率是否只统计 `Get`。
   - 吞吐是否按总操作数计算。

4. **观察波动范围**
   - 如果 `min~max` 很宽，先增加 `rounds` 再比较。

5. **交叉看“命中率 + 吞吐”**
   - 只看其中一个指标容易误判。

6. **检查是否出现“反直觉极端现象”**
   - 例如 Random 下某策略长期大幅领先。
   - 优先怀疑 workload/统计逻辑，再怀疑算法实现。

---

### 11.3 这份 benchmark 的“正常现象”参考

- `HotSet`：通常 LRU/ARC 表现较好。
- `ScanHot`：ARC/LFU 往往优于纯 LRU（因为更抗扫描污染）。
- `Random`：三者更接近通常是正常的。
- 吞吐方面：实现更简单的策略常常更快，但不代表综合表现一定最好。

---

### 11.4 当结果“明显不对劲”时，优先检查这三件事

1. trace 是否真的共享（同一条）
2. 命中率口径是否只算 `Get`
3. 是否做了 warmup + 多轮平均
