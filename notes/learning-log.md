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
