# KamaCache 学习日志（对话记录与推进流程）

> 目标：用“非常小的步子”从 0 手写一个线程安全缓存系统（LRU/LFU/ARC）。
> 原则：我自己写代码；助手只解释概念、给检查点与排错思路；每完成一步再追加下一步。

---

## 当前项目概况
- 项目名：`cacheStudy`（CLion 工程）
- 构建工具：CMake
- 代码目录：
  - `src/main.cpp`
  - `include/mycache/ICache.h`
  - `include/mycache/LRUCache.h`
  - `src/dummy.cpp`（用于让 `mycache` 成为真实库目标，方便 IDE 索引）

---

## 对话记录（按阶段）

### 阶段 A：确定学习路线与项目选择
**这次对话在说什么**
- 列出了知识星球一堆项目，你当前只会一点 C++ 基础，想知道先学哪个。

**目的**
- 选一个依赖最少、适合新手上手的项目作为起点。

**结论**
- 先从 **缓存系统（KamaCache）** 入手最合适；再逐步到内存池/协程/网络库等。

---

### 阶段 B：决定“从 0 重写项目”，并降低学习步长
**这次对话在说什么**
- 你希望从 0 完成 KamaCache，全程自己写；助手协助。
- 你提出：基础较弱，希望拆成更小步骤；且不要助手直接写代码。

**目的**
- 将大项目拆解为可执行的最小步骤，并明确协作方式。

**协作方式（约定）**
- 你写代码。
- 助手：解释概念、给小步骤任务、检查点（能否编译/运行/通过用例）、常见错误排查。

---

### 阶段 C：理解 CMakeLists.txt 在做什么
**这次对话在说什么**
- 你对 `CMakeLists.txt` 不熟，问每一行是干什么的。

**目的**
- 让你能看懂并做小范围修改（例如加源文件、设置标准、添加 include 路径）。

**要点总结**
- `cmake_minimum_required(...)`：要求 CMake 版本。
- `project(...)`：项目名与语言。
- `set(CMAKE_CXX_STANDARD ...)`：设置 C++ 标准。
- `add_executable(...)`：生成可执行程序目标。
- `target_include_directories(...)`：给目标添加头文件搜索路径（让 `#include "mycache/xxx.h"` 能被找到）。

---

### 阶段 D：是否需要专门学一遍 CMake？
**这次对话在说什么**
- 你问：要不要系统学一遍 CMake。

**目的**
- 给出能支撑当前项目的“CMake 最小知识集”。

**结论**
- 不用系统学完；掌握最常用的 5 类内容即可：
  1) 项目与 C++ 标准
  2) 目标（executable/library）
  3) include 路径
  4) 添加源文件
  5) 常用编译警告选项（可选）

---

### 阶段 E：CLion 提示头文件“不属于项目目标”的处理
**这次对话在说什么**
- 你发现打开 `ICache.h` / `LRUCache.h` 时，CLion 提示“此文件不属于任何项目目标”。
- 我们讨论原因：`target_include_directories` 只影响编译器找头文件，并不等于 IDE 会把头文件归到某个 target。

**目的**
- 让 CLion 将 `include/mycache/*.h` 识别为项目成员，确保索引/跳转/提示正常。

**采取的方案（B：拆库）**
1. 新增库目标 `mycache`，并让 `cacheStudy` 链接它。
2. 由于 `INTERFACE` 库有时 IDE 仍会保守，升级为 `STATIC` 库 + 添加 `src/dummy.cpp` 占位源文件。
3. 将头文件显式加入 `mycache` 的源文件列表（让 IDE 明确它们属于该 target）。

**检查点（你需要在 IDE 里做）**
- Reload CMake Project（或 Reset Cache and Reload）。
- 再打开 `include/mycache/ICache.h`/`LRUCache.h`，确认不再提示“不属于项目目标”。

---

### 阶段 F：接口 ICache 的模板声明、虚函数、纯虚函数、const 成员函数
**这次对话在说什么**
- 你开始写 `ICache` 的声明时，遇到了“模板声明写法”和“接口应该怎么写”的问题。
- 你问：为什么加上 `virtual` 还不够？纯虚函数是什么？
- 你问：为什么 `size()`/`capacity()` 要声明成 `const` 成员函数？

**目的**
- 建立“可替换缓存策略”的最小接口（ICache），为后续 LRU/LFU/ARC 奠定统一 API。

**要点总结**
- **虚函数（virtual function）**：允许通过“基类指针/引用”调用时，实际执行派生类实现（动态绑定）。虚函数 **可以有默认实现**。
- **纯虚函数（pure virtual function）**：在函数声明末尾写 `= 0`。
  - 含义：基类不提供（或不要求使用）实现，并且 **强制派生类实现**。
  - 含有纯虚函数的类是 **抽象类**，不能被直接实例化。
- **为什么接口更倾向用纯虚函数**：
  - 避免“基类只声明不实现导致链接错误（undefined reference）”。
  - 让 LRU/LFU/ARC 必须实现 put/get/erase 等 API，保证可替换性。
- **const 成员函数（例如 `size() const`）**：承诺不会修改对象逻辑状态。
  - `size()` / `capacity()` 语义上是“查询”，通常应为 `const`。
  - `get()` 在缓存里常常 **不是 const**（LRU 会更新访问顺序；LFU 会增加频次），这是正常的。

---

## Step 2 进展：LRUCache 骨架 + 有状态对象（capacity/size）

### 本次对话在说什么
- 你完成了 `LRUCache.h` 的骨架，在模板类里加入了：
  - 成员变量 `capacity_` / `size_`
  - 构造函数 `LRUCache(size_t capacity = 10)`
  - 为 `size()` / `capacity()` 添加 `[[nodiscard]]`（来自 Clang-Tidy 建议）
- 讨论了：模板类实现通常放在头文件（或 `.tpp/.ipp` 被 include），不建议放普通 `.cpp`。

### 目的
- 让 LRUCache 从“占位类”变成“有基本状态（容量/大小）”的可用对象，为下一步引入真实数据结构打基础。

### 已完成
- `LRUCache` 已能编译运行。
- `capacity()`/`size()` 返回有意义的成员值。

### 目前存在的规范/隐患（需要注意）
- `get()` 在 `size_ != 0` 的分支没有返回值（逻辑路径未返回）。这在开启更严格告警时可能会报错；也会导致未定义行为。
- `put/get/erase` 的参数暂时未使用，会触发“unused parameter`告警（等引入 map/list 后自然消失）。
- `[[nodiscard]]` 属于“接口友好型”规范：不是必须，但推荐（能提醒调用者别忽略返回值）。

### Checkpoint（验收点）
- [x] LRUCache 可以构造：`LRUCache<int,int> c(10)`
- [x] `capacity()` 返回 10
- [x] `size()` 初始为 0，put/erase 的占位逻辑能改变 size

---

## 下一步计划（Step 3）：引入真实 LRU 的数据结构（先单线程）

### 目标
- 不做线程安全，先实现“正确的 LRU 行为”。

### 需要引入的核心结构
- `std::list<...>`：维护访问顺序（头部最近，尾部最久）
- `std::unordered_map<K, list::iterator>`：O(1) 定位节点

### 行为定义（先统一语义）
- `put(key,value)`
  - key 已存在：更新 value，并把节点移动到表头
  - key 不存在：插入表头；若超容量，淘汰表尾并从 map 删除
- `get(key)`
  - 命中：返回 value，并把节点移动到表头
  - 未命中：返回 `std::nullopt`
- `erase(key)`
  - 存在则删除并返回 true，否则 false

---

## Step 4 进展：最小单元测试（不引入第三方）+ 用测试反推修复 LRU 逻辑

### 本次对话在说什么
- 你希望给 `LRUCache` 写“覆盖测试（coverage test）”，但没有用过断言，也没用过测试框架。
- 我们采用了最轻量的方式：**纯 C++ 自写断言 + 一个测试可执行程序**，并通过 CMake/CTest 接入工程。
- 你运行测试时遇到失败，我们根据失败信息定位到 LRU 的淘汰逻辑 bug，并修复后让全部测试通过。

### 目的
- 在继续做 LFU/ARC、以及后续线程安全/优化之前，先建立自动化回归验证：
  - 任何重构/优化都能快速知道“行为有没有被改坏”。

### 我做了什么（文件级别）
- `tests/test_lru.cpp`
  - 写了一个独立的测试可执行程序（带 `main()`）。
  - 加了最小断言宏：
    - `ASSERT_TRUE(expr)` / `ASSERT_FALSE(expr)`
    - `ASSERT_EQ(expected, actual)`
    - `ASSERT_HAS_VALUE(opt)` / `ASSERT_NO_VALUE(opt)`（用于 `std::optional`）
  - 写了 4 个测试用例：
    - `test_put_get_basic()`：put/get 基本行为 + size/capacity
    - `test_lru_eviction_order()`：访问顺序影响淘汰（LRU 核心）
    - `test_erase()`：删除存在/不存在 key
    - `test_put_same_key_does_not_grow_size()`：重复 put 同 key 不应 size++

- `CMakeLists.txt`
  - 开启测试：`enable_testing()`
  - 新增测试目标：`add_executable(lru_tests tests/test_lru.cpp)`
  - 链接缓存库：`target_link_libraries(lru_tests PRIVATE mycache)`
  - 注册 CTest：`add_test(NAME lru_tests COMMAND lru_tests)`

### 遇到的问题与解决
- **现象**：`test_lru_eviction_order()` 失败：期望 `get(2)` miss，但实际 hit。
- **原因（真实 bug）**：容量满时，你从 `list` 里 `pop_back()` 淘汰了尾节点，但没有同步从 `unordered_map` 删除被淘汰 key。
  - 这会让 `mp_` 里留下“指向已失效 list 节点的 iterator”，属于未定义行为（UB），可能表现为“错误命中/崩溃/随机错误”。
- **修复方向**：淘汰时要：先拿到 `lst_.back().first`（被淘汰 key）→ `mp_.erase(evictedKey)` → 再 `pop_back()`。

### 结果（Checkpoint）
- [x] `lru_tests` 可以运行
- [x] 失败信息能快速定位到具体行为
- [x] 修复后输出：
  - `Running LRUCache tests...`
  - `All tests passed.`

### 我学到了什么
- “断言”本质是：**条件不满足就立刻报错并终止**，在测试里用来判定对/错。
- 测试不是额外负担：它能快速暴露 **map/list 状态不同步** 这类隐藏 bug。
- 最轻量的测试方式也能带来很大收益：不必一开始就上 GoogleTest/Catch2。

### 下一步计划（测试学习的小目标）
- 使用 AAA（Arrange / Act / Assert）结构写测试：
  - Arrange：准备缓存与数据
  - Act：执行 put/get/erase
  - Assert：断言结果（值、size、淘汰）
- 继续给 LRU 补 3~5 条边界测试：
  - [x] capacity=1 的淘汰（已新增：`test_volumn_by_one_lru()`）
  - [x] 重复 get 不改变 size（已新增：`test_use_get_many_times_look_the_size()`）
  - [x] 覆盖写入是否更新 value 且变为最近使用
  - [x] erase 后再 put 的顺序是否正确
- 等测试数量明显增长后，再考虑引入 Catch2/GoogleTest（可选）。

---

## Step 5 进展：把 LRU 做成“更像库”的版本（Debug 不变量 + 单锁线程安全）

### 本次对话在说什么
- 你把 `LRUCache` 从“单线程实现”升级为“线程安全初版”。
- 从多把锁（capacity/size/list/map 分别一把锁）调整为 **单一大锁**（`mutable std::mutex mut_`）：
  - 每个 public API（`put/get/erase/size/capacity`）一进入就 `std::lock_guard` 上锁。
  - 解决了多锁方案里“锁顺序不一致导致死锁”的风险。
- 你加入了 Debug-only 的 `check_invariants_()` 进行不变量检查，并确保在持锁状态下调用。

### 目的
- 让 LRUCache 满足“可复用的小库”最重要的两个特性：
  1) 行为语义稳定（不变量守护）
  2) 并发调用不会破坏内部结构（互斥保护）

### 已完成
- [x] `capacity==0` 表示禁用缓存（put false / get nullopt / erase false / size=0）。
- [x] `put/get/erase` 全部持同一把锁，避免死锁。
- [x] Debug 下检查：`size_ == lst_.size() == mp_.size()` 且 `size_ <= capacity_`。
- [x] 现有单元测试全部通过。

### 待完善（下一步）
- 增加一个 **多线程冒烟测试**（不是性能测试）：
  - 多个线程并发随机执行 put/get/erase。
  - 目标：不崩溃、不触发 assert、不出现未定义行为。
  - 注意：冒烟测试不保证“业务结果必然一致”，它的价值是快速暴露数据竞争/锁遗漏。 

### Checkpoint（验收点）
- [x] Debug 构建运行 `lru_tests` 依然稳定。
- [ ] 新增并发冒烟测试后，重复运行多次也能通过。

---

## Step 5-B：多线程冒烟测试（Smoke Test）——理解每一行在做什么

### 为什么要写这个测试？
- 并发下很多 bug（数据竞争、迭代器失效、漏锁）是“概率性”的：单线程/小数据量很难跑出来。
- 冒烟测试不追求确定的业务结果，只追求：
  - 不崩溃
  - 不触发 `assert`（Debug 不变量）
  - 最终 `size() <= capacity()` 这种永远应该成立的性质依旧成立

### 测试参数是什么意思？
- `thread_count`：同时开多少个线程一起操作同一个 cache。
  - 越大并发越强，更容易暴露问题。
- `ops_per_thread`：每个线程做多少次 get/put/erase。
  - 越大测试时间越长，更容易把“偶发 bug”跑出来。
- `key_space`：随机 key 的取值范围（例如 0..63）。
  - 越小“撞 key”的概率越高，竞争越激烈；更容易测出问题。

### 为什么用 atomic<bool> start？
- 目的：让所有 worker 线程“同时起跑”，提升并发压力。
- 如果用普通 `bool start`：主线程写、worker 线程读会产生数据竞争（Undefined Behavior）。
- `std::atomic<bool>` 允许多线程安全地读写一个简单标志位。

### acquire / release（先用直觉理解）
- `start.store(true, std::memory_order_release)`：主线程发出“开始”信号。
- `start.load(std::memory_order_acquire)`：worker 等待直到看见“开始”。
- 直觉版理解：保证 worker 看到 start=true 时，也能“看见”主线程在发信号前完成的准备工作。
  （学习阶段不必深入内存模型细节，知道它是一对配合的同步语义即可。）

### worker 里随机数在做什么？
- `std::mt19937 rng(seed)`：伪随机数引擎（每个线程一个），避免多线程共享同一个随机数状态。
- `std::uniform_int_distribution<int> key_dist(a,b)`：生成 [a,b] 的均匀随机整数。
- `key_dist(rng)`：从分布里“抽样”一个随机 key。
- `op_dist(0,2)`：随机选择 0/1/2，分别代表 put/get/erase。

### 为什么有 while 自旋？
- `while(!start.load(...)) {}`：让 worker 线程先等在起跑线。
- 等主线程把 start 置 true 后，大家几乎同时开始跑。
- 自旋会短暂占用 CPU，但在测试里很常见、实现简单。

### 为什么用 vector<thread> + join？
- `std::vector<std::thread> threads;`：收集所有线程对象，统一管理。
- `threads.emplace_back(worker, t);`：创建线程并开始执行 `worker(t)`。
- `join()`：主线程等待所有子线程结束。
  - 线程对象若既不 join 也不 detach，在析构时会触发 `std::terminate()`，所以 join 是必需的。

### 最后一条断言在验证什么？
- 并发结束后 `ASSERT_TRUE(c.size() <= c.capacity());`
- 因为并发下最终 cache 里哪些 key 是不确定的，但“size 不超过容量”永远必须成立。

### 本次实践结果
- 我把 `ops_per_thread` 调到百万级（甚至更高）重复运行，测试仍稳定通过。
- 这给了我一个强信号：当前的“单锁 + 不变量检查”版本在并发场景下足够稳健。

---

## Step 6 进展：实现 LFUCache（分桶 + minFreq）并做成“库化 + 线程安全”版本

### 本次对话在说什么
- 从“我只知道 LFU 是按频次淘汰，但不知道怎么记录频次还能做到 O(1)”开始。
- 我们把 LFU 的核心数据结构拆解清楚，并在你现有工程风格（header-only 模板类、ICache 统一接口、单大锁、Debug 不变量）下落地。
- 过程中重点讨论了：`minFreq_` 为什么一定要维护、`erase()` 为什么会让 `minFreq_` 失效、以及如何在不扫描全表的前提下修复。

### 目标（Contract）
- **功能语义**：
  - `put(key, value)`：写入/更新，并视为一次访问（会提升 freq）。
  - `get(key)`：命中返回 value，并提升 freq；miss 返回 `std::nullopt`。
  - `erase(key)`：存在则删除并返回 true；不存在返回 false。
  - `capacity==0`：视为“禁用缓存”（put=false / get=nullopt / erase=false / size=0）。
- **复杂度目标**：在典型实现下 `get/put/erase` 期望均摊 O(1)（利用哈希表 + 链表），淘汰定位依赖 `minFreq_`。
- **工程目标**：
  - 多线程安全（先用“单大锁”保证正确性）。
  - Debug-only 不变量检查（尽早发现 map/list 不同步）。

### LFU 为什么能做到 O(1)：两层哈希 + 链表
**核心结构（你最终写出来的样子）**：
- `nodes_ : unordered_map<K, Node>`
  - `Node` 保存：`value`、`freq`、以及 **指向频次桶链表的迭代器** `it`。
- `freq_list_ : unordered_map<int, list<K>>`
  - `freq -> keys`，其中 `list` 的：
    - `front` 表示该 freq 下 MRU
    - `back` 表示该 freq 下 LRU（满时淘汰用）
- `minFreq_`：当前缓存中“最小频次”的候选值，用于 O(1) 定位淘汰桶。

**关键手法：`touch_(key)`**
- 命中后把 key 从 freq=f 的桶移到 freq=f+1 的桶，并放到新桶表头。
- 因为节点保存了 `list<K>::iterator`，所以从旧桶 `erase` 是 O(1)。

### `minFreq_` 的坑：为什么必须维护 / 为什么 `erase()` 会破坏它
我们明确了一个事实：
- **淘汰只在 `put(newKey)` 且 `size_ == capacity_` 时发生**。
- 但 `erase()` 可能导致：
  - 删空了 `minFreq_` 对应的桶
  - `minFreq_` 仍然指向一个“已经不存在的桶”
  - 下一次满容量淘汰时，`freq_list_.find(minFreq_)` 会失败（Debug 下 assert 崩，Release 下可能 UB）。

### 为什么不直接用“小根堆”维护最小频次？
我们讨论结论是：
- 堆会把命中时的 `freq++`（以及同 freq 的 LRU tie-break）变成 **改键操作**。
- 标准 `std::priority_queue` 不支持 decrease/increase-key，需要：
  - 要么自己实现索引堆（O(log n)）
  - 要么 lazy deletion（会堆积过期节点，淘汰时可能弹很多次）
- 而分桶法 + `minFreq_` 的经典优势就是：命中移动 O(1)，淘汰定位 O(1)/均摊 O(1)。

### 你最终采用的 `minFreq_` 修复策略（工程上最省心）
- **结论**：允许 `minFreq_` 在某些时刻“不精确”，但必须保证“淘汰前它指向一个存在的桶”。
- 做法：在 `put()` 进入淘汰分支前，用单调推进修复：
  - `while (freq_list_.find(minFreq_) == freq_list_.end()) ++minFreq_;`
- 这比每次扫描 `freq_list_` 找最小值更好：
  - 扫描是稳定 O(#buckets)
  - 单调推进在整体操作序列上通常是均摊 O(1)

### 线程安全：为什么必须加锁
- 多线程下如果不加锁：`unordered_map`、`list`、以及迭代器都会在并发修改中失效，属于数据竞争（Undefined Behavior），可能随机崩溃。
- 你采用了和 LRU 一致的工程方案：**单大锁**
  - 在 `put/get/erase/size/capacity` 一进入就 `std::lock_guard<std::mutex>`。
  - `touch_()` 只在持锁状态下调用。

### Debug-only 不变量与 Clang-Tidy 告警
你给 LFU 也加了 `check_invariants_()`，核心检查：
- `size_ == nodes_.size()`
- `size_ <= capacity_`
- `freq_list_` 中 key 总数之和等于 `size_`

并且我们遇到一个典型告警：
- **Clang-Tidy: Side effect in assert() condition discarded in release builds**
- 原因：`assert` 在 Release 会被移除，写在 `assert(++steps <= limit)` 里的 `++steps` 属于副作用，Release 下会消失。
- 修复：把副作用挪出 assert：
  - `++steps; assert(steps <= limit);`

### 本阶段产出（你已完成）
- [x] `include/mycache/LFUCache.h`：分桶 LFU + `minFreq_` + 单锁 + Debug 不变量
- [x] 解决 `minFreq_` 可能失效导致的淘汰断言崩溃问题（淘汰前推进）
- [x] 解决 Clang-Tidy assert 副作用告警（将 `++steps` 独立出来）

### 下一步建议
- 给 LFU 补齐与 LRU 同级别的单元测试（先 4~6 个覆盖用例即可）：
  - 基本 put/get
  - 频次淘汰
  - 同频次下的 LRU 淘汰
  - put 已存在 key 会提升 freq
  - erase 行为 + erase 后再 put 的边界
  - capacity==0

---

## Step 7：给 LFUCache 做“库化注释” + 完善测试注释 + 测试分层

### 这次对话在做什么
- 你已经把 `LFUCache` 的实现（分桶 + `minFreq_` + 单锁 + Debug 不变量）跑通，并且 LFU 的专项测试也稳定通过。
- 接下来我们做的是“工程化收尾”：
  1) 让 `include/mycache/LFUCache.h` 更像一个可复用的小库（清晰的契约、复杂度、线程安全说明、关键数据结构解释）。
  2) 给三份测试文件补上更详细的中文注释，方便未来扩展/回归。
  3) 把这些内容记录进学习日志。

### 目标（为什么要写这么多注释）
- 对新手来说：
  - 注释相当于“读代码的导航”，能把实现背后的设计意图写清楚。
- 对工程来说：
  - 未来你做 ARC、分片缓存、性能优化时，注释里的“契约/不变量/复杂度”就是你判断“改动有没有破坏行为”的依据。

### LFUCache.h 的“库化注释”做了什么
文件：`include/mycache/LFUCache.h`

补充了以下信息：
- **策略说明**：LFU 优先淘汰最小频次；同频次按桶内 LRU 淘汰（back）。
- **复杂度声明**：get/put/erase 期望 O(1)（建立在 `unordered_map` 均摊 O(1) 上）。
- **线程安全语义**：
  - 单把大锁 `mtx_` 保护所有公开 API。
  - `touch_()` 只在持锁时调用，因此本身不加锁。
- **capacity==0 的语义**：明确“禁用缓存”行为（put=false/get=nullopt/erase=false/size=0）。
- **Debug-only 不变量**：
  - `size_ == nodes_.size()`
  - `size_ <= capacity_`
  - `freq_list_` 中 key 数量总和 == `size_`
  - `size_==0` 时 `minFreq_==0` 且结构为空
  - `size_>0` 时 `minFreq_` 必须能在 `freq_list_` 中找到

### minFreq_ 的维护策略（我们最终落地的版本）
- 我们最终采取的是：
  - `touch_()`：当旧桶空 && 旧桶是 `minFreq_` 时，直接 `minFreq_ = f + 1`。
  - `erase()`：当删除导致桶空且桶是 `minFreq_` 时，扫描 `freq_list_` 找新的最小频次（`O(#buckets)`），保证强不变量。
- 这样做的好处：
  - `put()` 的淘汰路径可以直接 `find(minFreq_)`，不需要 while 懒修复。
  - Debug 下更容易保证“不变量总是成立”，排错更快。

### 测试的分层（建议长期保持）
我们把测试分成两层：

1) **接口通用测试（Contract Test）**：
   - 文件：`tests/test_icache_basic.cpp`
   - 作用：只验证 `ICache` 的基础语义，与策略无关。
   - 典型用例：capacity==0、覆盖写 size 不变、erase 行为、基本 put/get。

2) **策略专项测试（Policy Test）**：
   - LRU：`tests/test_lru.cpp`
   - LFU：`tests/test_lfu.cpp`
   - 作用：验证策略核心行为（淘汰顺序、tie-break 规则等）。

3) **并发 smoke test（冒烟）**：
   - LRU/LFU 都有随机 put/get/erase 的并发用例。
   - 只断言不变量（例如 `size() <= capacity()`）与“不崩溃”。
   - 不做确定性淘汰断言（并发调度不可控，会让测试脆弱）。

### 这次你应该记住的工程经验
- “库化”的核心不是把代码变复杂，而是把 **契约（Contract）** 写清楚：
  - 输入/输出语义
  - 边界条件（capacity==0）
  - 线程安全保证
  - 不变量与复杂度
- 测试也要“库化”：
  - 通用语义写一份复用
  - 策略差异单独测
  - 并发只做 smoke，不做 deterministic 断言

### 下一步建议
- 如果你准备继续往 ARC / 分片缓存走：
  - 先把“测试宏”抽成一个公共头（例如 `tests/test_common.h`），避免三份测试重复写宏（这属于低风险重构）。
  - 或者继续保持现状也没问题；当测试越来越多时再抽。



---

## Step 8 进展：开始实现 ARC（Adaptive Replacement Cache）— put/replace/ghost 裁剪 + 学会 list::splice

> 说明：本阶段仍坚持“我自己写代码，助手只做审阅与引导”。ARC 的 `ARCCache.h` 目前属于单线程版本（尚未加锁），并且 `get/erase` 还在开发中。

### 8.1 先搞清 ARC 是什么（不是 LRU+LFU 的简单拼接）
**核心概念（四个 LRU 列表）**
- `T1`：真实缓存（有 value），偏“最近性”（新进入/只命中过一次的条目）
- `T2`：真实缓存（有 value），偏“频繁性”（二次及以上命中的条目）
- `B1`：ghost（只存 key），记录“最近从 T1 淘汰”的痕迹
- `B2`：ghost（只存 key），记录“最近从 T2 淘汰”的痕迹

**关键不变量/约束**
- 真实缓存容量：`|T1| + |T2| <= c`
- 全部痕迹总量：`|T1| + |T2| + |B1| + |B2| <= 2c`

**自适应参数 `p` 的含义**
- `p` 是“目标上给 T1 的配额（倾向最近性）”，范围 `0..c`。
- 幽灵命中时调整：
  - 命中 `B1`（说明最近性不够）→ `p` 增大（更偏 LRU）
  - 命中 `B2`（说明频繁性不够）→ `p` 减小（更偏 LFU-ish）
- `p` 的调整与“真实缓存是否满”无关；满与否只决定是否需要 `replace()` 腾位置。

---

### 8.2 put 的结构化写法：必须拆成 5 路互斥分支
我们把 `put(key,value)` 拆成 5 路互斥分支，避免 `if (it1==end || it2==end)` 这种很绕的条件：
1) `key ∈ T1`：更新 value，并把节点从 `T1 → T2`（二次命中升级为频繁段）
2) `key ∈ T2`：更新 value，并把节点移动到 `T2` 头部（MRU）
3) `key ∈ B1`：调整 `p`（增大），必要时 `replace()`，从 `B1` 删除 key，然后插入到 `T2`
4) `key ∈ B2`：调整 `p`（减小），必要时 `replace()`，从 `B2` 删除 key，然后插入到 `T2`
5) 四表都 miss：必要时 `replace()`，插入到 `T1`

> 统一规则：**只要接下来要插入一个“真实条目”（进 T1/T2），就先检查是否真实满**：
> - 若 `|T1|+|T2| == c`：必须 `replace()`
> - 否则不需要

---

### 8.3 学会 std::list::splice：缓存实现里最重要的 O(1)“移动节点”工具
**为什么不用 `std::move` 解决“命中后移动”？**
- `std::move` 移的是“对象内容”，不是链表节点。
- LRU/LFU/ARC 的关键是“命中后把节点挪到 MRU”，应该移动节点指针（O(1)），而不是拷贝/移动 pair。

**splice 的两种缓存常用场景**
- 同 list 内移动到头部（touch）：把某个 iterator 节点挪到 `begin()`
- 跨 list 移动（T1 → T2）：把 iterator 从 `T1` 剪下来，挂到 `T2.begin()`

**易错点**
- `splice` 要明确 source list；同 list 也要用“三参数形式”。
- 跨 list splice 后，必须更新对应 map：
  - 从 source map erase
  - 在 dest map 写入新 iterator

---

### 8.4 为什么谨慎使用 unordered_map::operator[]（不是不能用，而是容易埋雷）
- `mp[key]` 在 key 不存在时会“悄悄插入默认值”，在缓存的多表状态机里容易隐藏 bug。
- 当我们还在开发期时，更推荐：
  - `find` 先判断是否存在
  - 或者 `insert_or_assign/try_emplace`
- 但当我们配套了 Debug-only 不变量检查后，`operator[]` 的风险会被大幅降低（因为错误状态会更早暴露）。

---

### 8.5 replace()：ARC 的“真实淘汰”函数（只做一件事）
**replace 的职责（契约）**
- 只在 `|T1| + |T2| == c`（真实满）时被调用
- 从 `T1` 或 `T2` 淘汰 1 个真实条目，并把淘汰的 key 放入对应 ghost：
  - 从 `T1` 淘汰（LRU/back）→ key 进 `B1`（MRU/front）
  - 从 `T2` 淘汰（LRU/back）→ key 进 `B2`
- `replace()` 不修改 `p`。

**决策规则（第一版够用）**
- 若 `!T1.empty()` 且 `|T1| > p`：淘汰 T1
- 否则淘汰 T2（若 T2 为空则退化淘汰 T1）

**实现细节要点**
- 先取 victim key，再更新 list/map，避免 `back()` 多次取值导致顺序风险。
- ghost 侧也要维护 `b?_mp_[key] = b?.begin()`（映射必须是 iterator，不是 bool）。

---

### 8.6 ghost 裁剪 repeat_ghost()：保证 total <= 2c
**为什么要有 ghost 裁剪？**
- B1/B2 不存 value，但它们是“历史痕迹”，数量必须受控，否则会无限增长。

**工程上最稳的策略（我们采用的方向）**
- `while (total > 2*c)`：每次删一个 ghost
- 删除哪一个？优先删更大的那一边（维持大致平衡）：
  - `if (|B1| > |B2|) pop B1.back() else pop B2.back()`
- pop 时必须同步：`ghost_mp.erase(key)` + `ghost_list.pop_back()`

**重要提醒（debug 上容易踩坑）**
- 如果 `total <= 2c`，trim 应立即 return，不能 assert 误伤。
- 如果进入 while 循环，理论上 `B1` 或 `B2` 至少有一个非空；断言应放在循环内部或写成更严格的防御。

---

### 8.7 Debug-only 不变量：帮助快速发现“map/list 不同步”
本阶段写了几类 Debug-only 检查：
- key 唯一性：同一个 key 不能同时存在于 `T1/T2/B1/B2` 的多个 map
- iterator 正确性：map 中的 iterator 解引用后应指向同 key/value

**经验**
- Debug-only 的断言必须避免 UB：先判断 `find != end()` 再解引用。
- 断言是“报警器”，不应过早追求完美；关键路径（replace/put）跑通后再逐步补强。

---

### 8.8 下一步（主线）
1) 实现 `get(key)`（5 路分支：T1/T2 hit、ghost hit、miss）
2) 实现 `erase(key)`（至少支持删 T1/T2；是否处理 ghost 取决于策略）
3) 新增 ARC 专项测试：
   - put/get 基础命中
   - T1→T2 升级行为
   - B1/B2 命中对 p 的影响（可用黑盒方式间接验证淘汰倾向变化）
   - total 不变量（`<= 2c`）在大量 put 下依然成立


---

## Step 9：完成 ARCCache 的 get/erase + 接入 ICache 基础测试 + ARC 专项测试（含压力回归）

> 本阶段关键词：ARC 的 ghost 语义、get 的返回值契约、erase 的工程语义（显式失效）、Debug-only 断言如何正确使用、以及如何把 ARC 加入测试与构建体系。

### 9.1 我们解决了什么问题（本次对话在做什么）
- 你完成了 ARC 的 `replace()` 与 `put()` 主流程后，开始实现 `get()` 与 `erase()`。
- `get()` 初版出现了“命中却返回空 optional”的严重逻辑错误，我们把它修正为：
  - `T1/T2 hit`：返回 value，并维护 T1→T2 升级 / T2 的 MRU
  - `B1/B2 hit`：**返回 miss（nullopt）**，但会调整自适应参数 `p_`
  - `miss`：返回 nullopt
- `erase()` 的语义一开始不明确，我们先“定契约”，再按四表落地删除逻辑。
- 当你把 ARC 接入 `icache_basic_tests` 后，又触发了一个 Debug-only 断言崩溃，我们定位并修复。
- 最后新增了 `arc_tests`（专项测试）并加入一个确定性的压力回归用例。

### 9.2 ARC 里为什么 ghost 命中仍然是 miss？（非常关键的语义）
ARC 的四个表：
- **真实缓存**：T1 / T2（存的是 `(K,V)`）
- **幽灵缓存**：B1 / B2（只存 `K`，不存 `V`）

因此：
- `get(key)` 命中 T1/T2 才能返回 value。
- `get(key)` 命中 B1/B2 本质上是：
  - “我记得你来过，但我已经没有数据了”
  - 所以 API 上仍然要返回 `std::nullopt`
  - 但它会作为反馈信号，用来调整 `p`（ARC 的“自适应”来源）

一句话记忆：
> **ARC 中：B1/B2 hit 是“带反馈信号的 miss”。**

### 9.3 get() 的典型坑与修复点
- **坑 1：命中后没返回值**
  - 现象：T1/T2 hit 也返回空 optional
  - 修复：在 splice/move 之前或之后把 `V` 取出来，然后 `return value`。
- **坑 2：Debug-only 检查写在 miss 分支**
  - 你写了 `check_key_in_mp_only_(key)`，其契约是“key 必须且只能在四表之一”。
  - 但 miss 场景下 key 就应该在 0 个表里，导致 `c==0`，断言炸。
  - 这在 `icache_basic_tests` 中必然触发，因为通用测试会做“get 不存在 key”。

工程经验（Debug-only 断言契约要和调用点一致）：
- `check_key_in_mp_only_(key)`：只应在“你已知 key 必存在于某个表”时调用。
- miss 分支要么不检查，要么单独提供 `check_key_absent_(key)`。

### 9.4 erase() 的工程语义：显式失效（invalidate）要“彻底移除”
我们最终建议并采用的语义是：
- `erase(key)`：无论 key 在 `T1/T2/B1/B2` 的哪张表出现，都把它彻底移除。
- `erase()` 不调整 `p`（`p` 的自适应只由访问模式驱动）。
- 返回值：
  - 删除到任何一个条目就返回 `true`
  - 四表都没有则 `false`

原因：
- 工程里 `erase` 一般表示“外部显式删除/失效”，不应留下 ghost 继续影响自适应，否则调用者会困惑。

### 9.5 ARC 专项测试（arc_tests）的测试分层思路
我们保持与 LRU/LFU 一致的测试分层：
- `icache_basic_tests`：接口契约测试（对 LRU/LFU/ARC 都跑）
- `arc_tests`：ARC 策略专项测试（验证 ghost 语义、erase 语义、以及基本健壮性）

本阶段新增的 ARC 测试覆盖：
- **erase 删除真实条目**：T1/T2 场景
- **ghost 语义**：构造 replace 后的 miss key，验证 get 仍然 miss；erase 能清掉该 key
- **size 不变量**：混合 put/get/erase 下始终满足 `size() <= capacity()`
- **确定性压力回归**：固定随机种子下的随机操作序列（可重复、可回归）

注意：压力测试里我们不做“确定淘汰顺序”的断言，只断言强不变量：
- 不崩溃
- `size <= capacity`
- put 后立刻 get 同 key 必须读回刚写入的值（cap>0）

### 9.6 构建/工程补强（为了更像一个库）
- 聚合头 `include/mycache/mycache.h` 补齐导出：
  - 新增 `#include "mycache/ARCCache.h"`
- `CMakeLists.txt` 新增 ARC 测试目标：
  - `add_executable(arc_tests tests/test_arc.cpp)`
  - `target_link_libraries(arc_tests PRIVATE mycache)`
  - `add_test(NAME arc_tests COMMAND arc_tests)`

### 9.7 小结：本阶段你真正掌握的东西
- ARC vs LRU/LFU 的本质区别：B1/B2 ghost + 自适应参数 p。
- `std::optional` 的契约写法：
  - 命中就返回 value
  - miss 返回 `std::nullopt`
- Debug-only 断言要“契约匹配”：不要在 miss 分支用“必须存在”的断言。
- 测试分层（Contract tests + Policy tests + Stress smoke）是工程里很常用的结构。

### 9.8 仍然欠缺/后续可以做的方向
- （可选）ARC 线程安全：像 LRU/LFU 一样加单大锁，以及对应的并发 smoke test。
- 性能对比 benchmark：LRU/LFU/ARC 在同一访问序列下对比命中率与吞吐。
- 进一步的 Debug-only 全局不变量：验证 map/list 结构一致性（iterator 指向正确节点）。

---

## Step 10：Benchmark 基准测试初版（LRU / LFU / ARC 命中率与吞吐对比）

> 本阶段目标：先把 benchmark 的“完整骨架”搭起来——不是追求一次性做成最终版，而是先拥有一个**可运行、可比较、可继续扩展**的性能对比程序。

### 10.1 这次对话在说什么
- 你希望把缓存系统项目补上 benchmark，对 `LRU / LFU / ARC` 在**同一条访问序列**下进行公平对比。
- 我们先明确 benchmark 的契约，再把 `src/bench_cache.cpp` 补成一个完整的基准程序。
- 同时把这部分内容整理进学习日志，方便你后面反过来学习“为什么这样设计”。

### 10.2 Benchmark 的核心原则：先保证“公平”，再谈“快不快”
缓存策略比较里，最容易出问题的不是代码写不出来，而是**比较口径不一致**。

我们这次先固定了几个原则：
1. **同一条 trace 跑所有 cache**
   - LRU、LFU、ARC 必须吃同一份操作序列，才能公平比较。
2. **每个 cache 都从空状态开始**
   - 不能让前一个策略跑完后的状态影响下一个策略。
3. **hit rate 只统计 get**
   - `put / erase` 不参与命中率分母。
   - 否则“命中率”会掺入写入比例，指标会失真。
4. **吞吐量按总操作数统计**
   - 使用 `total_ops / elapsed_time`，直观反映整体处理速度。
5. **固定随机种子，保证可复现**
   - benchmark 结果允许有微小波动，但 workload 本身应可重复生成。

### 10.3 先拆 benchmark 的三个层次
为了不一上来就把代码写乱，我们把 benchmark 拆成三层：

#### 第 1 层：Workload 生成层
作用：生成一条“访问序列 trace”。

当前已有两个 workload：
- `createHotSetTrace(...)`
  - 模拟热点访问（hot set）
  - 适合看缓存对高局部性读流量的表现
- `createScanHotTrace(...)`
  - 模拟扫描干扰 + 热点访问
  - 更适合看 ARC 这类策略对 scan pollution 的抵抗能力

#### 第 2 层：执行层（runner）
作用：把一条 trace 逐条喂给某个 cache，并记录统计结果。

这个阶段新增了：
- `BenchmarkResult`
- `runBenchmark(...)`

它负责：
- 遍历 trace
- 调用 `cache.get / put / erase`
- 统计 `get_ops / put_ops / erase_ops`
- 统计 `hits / misses`
- 记录耗时

#### 第 3 层：展示层（reporting）
作用：把 benchmark 结果打印成人能看懂的表格。

这个阶段新增了：
- `printResultHeader()`
- `printResultRow(...)`

这样做的好处：
- 后续如果你想把输出改成 CSV / Markdown / JSON，就不用改 workload 和 runner。

### 10.4 为什么 `BenchmarkResult` 要单独做成结构体？
因为 benchmark 不是“跑完就算”，而是要留下结果做比较。

当前结构体里放了这些字段：
- `cache_name`
- `workload_name`
- `total_ops`
- `get_ops / put_ops / erase_ops`
- `hits / misses`
- `elapsed_ms`

并提供两个派生指标函数：
- `hit_rate()`
- `throughput_ops_per_sec()`

这种写法的工程好处：
- **统计原始数据** 和 **计算展示指标** 分开
- 将来你要新增：
  - 平均每次操作耗时
  - p50/p95/p99
  - 写命中率 / 读命中率
  都比较容易扩展

### 10.5 为什么 `runBenchmark(...)` 的参数设计成这样？
当前形式：
- `cache_name`
- `ICache<int,int>& cache`
- `const Trace& trace`
- `workload_name`

这是一个很典型的“策略无关 benchmark 执行器”。

它的设计重点是：
- **只依赖 ICache 接口**
  - 这体现了你前面做接口抽象的价值。
  - 后续你再加新算法时，只要实现 `ICache`，benchmark 不需要重写。
- **trace 作为只读输入**
  - 所有策略共享一份操作序列，保证公平。
- **每次运行都返回一个结果对象**
  - 方便汇总、排序、打印、导出。

### 10.6 这次补上的边界处理
在写 benchmark 时，我们顺手修了几个 trace 生成的边界问题：

#### `createHotSetTrace(...)`
新增防御：
- `ops == 0` 直接返回空 trace
- `key_space == 0` 直接返回空 trace
- `hotset_size == 0` 时，不走热点分支，统一退化到全空间随机

#### `createScanHotTrace(...)`
新增防御：
- `ops == 0` 直接返回空 trace
- `key_space == 0` 直接返回空 trace
- `scan_batch_size == 0` 时强制修正为 1
- `hotset_size == 0` 时，跳过“插入热点访问”阶段，退化为纯 scan 流量

这些边界处理的意义：
- benchmark 是“实验工具”，越不容易因为奇怪参数崩掉越好。
- 你以后可能会反复调参数，边界处理能省很多排错时间。

### 10.7 当前 main 的职责：先做一个“最小可运行 benchmark”
`main()` 现在不再只是打印随机数，而是完成了最小可运行流程：

1. 定义一份 `BenchmarkConfig`
2. 固定随机种子
3. 生成两条 trace：
   - HotSet
   - ScanHot
4. 分别创建：
   - LRU
   - LFU
   - ARC
5. 对两条 trace 分别跑三种策略
6. 输出结果表格

这一步非常重要，因为它标志着：
> 你的 benchmark 已经从“想法”变成“可执行程序”了。

### 10.8 这版 benchmark 还不是什么？
虽然它已经能工作，但它还不是最终版，当前還沒有做這些高級內容：
- 多轮重复运行取平均值
- 预热（warmup）与正式计时分离
- 不同容量的 sweep（例如 64 / 128 / 256）
- CSV/JSON 导出
- 更复杂的 workload（例如 Zipf 分布）
- 并发 benchmark（当前还是单线程策略比较）
- 延迟分位数（p50 / p95 / p99）

所以你可以把它理解为：
- **benchmark v1：结构完整、指标清楚、可继续扩展**
- 不是最终性能实验平台

### 10.9 这一阶段真正值得学的工程点
1. **先定义指标，再写代码**
   - 命中率怎么算？吞吐量怎么算？要先定契约。
2. **把 benchmark 分层**
   - workload / runner / reporting 分开，比全写在 main 里好维护得多。
3. **统一接口的价值**
   - `ICache` 让 LRU/LFU/ARC 能被同一个 runner 驱动。
4. **可复现性很重要**
   - 固定 seed 是 benchmark 的基本素养。
5. **边界参数要防御**
   - benchmark 代码也是工程代码，不只是临时脚本。

### 10.10 下一步建议（你学习时可以按这个顺序看）
当你准备真正“学习如何自己写这部分代码”时，可以按这个顺序来：

1. 先只看 `TraceEntry / Trace / OpType`
   - 理解“访问序列”为什么要先抽象出来
2. 再看两个 trace 生成函数
   - 重点理解 workload 在模拟什么业务场景
3. 再看 `BenchmarkResult`
   - 思考为什么这些字段足够支撑当前比较
4. 最后看 `runBenchmark(...)`
   - 它就是 benchmark 的核心执行器
5. 最后再看 `main()`
   - 它只是把前面这些模块串起来

一句话总结这一步：
> 我们先把 benchmark 搭成了一个“可以跑、可以看、可以继续演化”的最小框架，后面你再来学习每一层为什么这样写。

### 10.11 新增第三种 workload：Random（完全随机）
为了让 benchmark 更完整，我们又补了一种最基础的基线负载：

- `createRandomTrace(...)`

它的特点是：
- key 完全均匀随机落在 `[0, key_space)`
- 操作类型也随机：
  - `read_ratio` 概率生成 `Get`
  - 剩余写操作里，90% 是 `Put`
  - 10% 是 `Erase`
- 没有热点
- 没有扫描模式
- 没有额外的访问局部性设计

#### 为什么这个 workload 很重要？
因为前两个 workload 都带有“结构性”：
- `HotSet`：偏高局部性
- `ScanHot`：偏扫描干扰 + 热点混合

而 `Random` 更像一个“基线场景”：
- 如果访问模式几乎没有局部性
- 那么各种缓存策略通常都不会表现得特别好
- 命中率一般会明显低于 `HotSet`

所以它的价值不是“让某个策略赢”，而是：
> 给 benchmark 提供一个没有明显偏向的参照组。

#### 三种 workload 现在分别在测什么？
1. **HotSet**
   - 测试缓存面对高热点访问时的表现
   - 命中率通常较高
2. **ScanHot**
   - 测试缓存面对扫描污染时的抗干扰能力
   - ARC 往往在这类场景更有优势
3. **Random**
   - 测试在弱局部性、近乎纯随机访问下的基线表现
   - 更适合作为“保底参照”而不是“优势场景”

#### 工程上的小变化
- `main()` 现在会生成第三条 trace：`random_trace`
- benchmark 结果从原来的 6 行（2 workloads × 3 caches）变成 9 行（3 workloads × 3 caches）
- `results.reserve(...)` 也同步从 6 改成 9

这样一来，当前 benchmark v1 已经具备了三类典型模式：
- 有热点
- 有扫描干扰
- 纯随机基线

这会让后面你分析命中率与吞吐时，结论更有说服力。

---

## Step 10-B：Benchmark 完整版（多轮 + 预热 + 汇总 + 讲解文档）

> 本阶段目标：把 benchmark 从“能跑”升级为“更可信、可解释、便于学习复盘”的版本。

### 10-B.1 这次做了什么
- 在 `bench_cache.cpp` 中补齐了多轮 benchmark 流程：
  - `warmup_rounds`（预热轮，不计入结果）
  - `rounds`（正式计时轮）
- 新增多轮汇总结构 `BenchmarkSummary`：
  - 命中率/耗时/吞吐都给出 `avg + min + max`
- benchmark 主流程改为“workload × cache”的双层循环：
  - 避免重复代码，后续新增 workload 或策略更轻松
- 输出表头与行格式做了增强：
  - 增加 `Gets / Puts / Erases`
  - 增加 `Hit(min~max)` 与 `Thr(min~max)`
- 修正了范围输出的格式化方式：
  - 从 `to_string + substr` 改为统一格式化函数（更稳定、更易读）

### 10-B.2 为什么要加 warmup 和多轮
单次 benchmark 常见问题：
- 冷启动影响大（缓存对象刚创建、运行时状态未稳定）
- 调度/系统噪声会造成偶发抖动

改成“预热 + 多轮”后：
- 结果更稳定
- 观察波动范围更直观
- 更容易判断“策略差距是真差距还是噪声”

### 10-B.3 这版 benchmark 的工程价值
这次不是简单“加功能”，而是把 benchmark 变成可复用的小框架：

1. **可复现**：固定 seed + 固定参数
2. **可解释**：输出口径清晰（命中率只看 Get，吞吐看总操作）
3. **可扩展**：新增 workload/策略时只需补描述与构造，不用复制大段主流程
4. **可教学**：代码分层（生成 / 执行 / 汇总 / 输出）清晰

### 10-B.4 新增配套文档
新增：`notes/benchmark-guide.md`

文档覆盖：
- benchmark 的问题定义
- 三种 workload 的意义
- 关键函数职责和设计理由
- C++ 特性点（optional / chrono / [[nodiscard]] 等）
- 后续扩展方向（capacity sweep、CSV 导出、Zipf workload）

### 10-B.5 目前 benchmark状态（阶段结论）
- [x] HotSet / ScanHot / Random 三种 workload
- [x] LRU / LFU / ARC 同 trace 公平对比
- [x] warmup + 多轮 measured 汇总
- [x] avg + min/max 输出
- [x] 学习型讲解文档落地

一句话总结：
> benchmark 已经从“实验草稿”升级为“可复现、可解释、可继续演化”的工程化版本。
