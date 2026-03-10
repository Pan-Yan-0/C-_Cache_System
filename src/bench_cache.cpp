//
// Created by py on 26-3-8.
//

#include "mycache/mycache.h"
#include <chrono>
#include <vector>
#include <iostream>
#include <string>
#include <memory>
#include <iomanip>
#include <limits>
#include <sstream>

// Benchmark 目标：在“同一访问序列”上比较 LRU / LFU / ARC 的命中率与吞吐。
// 这个文件只做单线程 benchmark：
// 1) 先生成 trace（访问序列）
// 2) 再让不同缓存策略重复执行同一条 trace
// 3) 统计 hit rate / throughput，并做多轮汇总

enum class OpType {
    Get,
    Put,
    Erase
};

// 一条操作记录：type 表示操作类型，key 表示访问哪个 key，
// value 仅在 Put 时有意义（Get / Erase 时占位为 0）。
struct TraceEntry {
    OpType type;
    int key;
    int value; // 对于 Get/Erase 可能 unused，但为了统一结构保留
};

using Trace = std::vector<TraceEntry>;

// Workload A：HotSet
// - 大部分访问集中在较小热点区域
// - 适合观察“最近性/局部性”较强场景下，各策略的表现
Trace createHotSetTrace(size_t ops, size_t key_space, size_t hotset_size,
                        double hot_rate, double read_ratio, unsigned int seed) {
    Trace trace;
    trace.reserve(ops); // 预分配内存以提高性能

    if (ops == 0 || key_space == 0) return trace;
    if (hotset_size > key_space) {
        hotset_size = key_space;
    }

    for (size_t i = 0; i < ops; ++i) {
        // 1. 决定操作类型 (Read vs Write)
        // 生成 [0.0, 1.0) 之间的随机浮点数
        double r = static_cast<double>(rand_r(&seed)) / RAND_MAX;

        bool is_read = (r < read_ratio);

        // 2. 决定 Key (Hot vs Cold)
        // 生成另一个随机数来决定是否访问热点
        double k_rand = static_cast<double>(rand_r(&seed)) / RAND_MAX;
        int key;

        if (hotset_size > 0 && k_rand < hot_rate) {
            // 命中热点: [0, hotset_size)
            key = rand_r(&seed) % static_cast<int>(hotset_size);
        } else {
            // 未命中热点 (冷数据): [0, key_space)
            key = rand_r(&seed) % static_cast<int>(key_space);
        }

        if (is_read) {
            // Get 操作
            trace.push_back({OpType::Get, key, 0});
        } else {
            // 写操作：90% Put, 10% Erase
            double write_dist = static_cast<double>(rand_r(&seed)) / RAND_MAX;

            if (write_dist < 0.9) {
                // Put 操作，value 使用递增计数器 i
                trace.push_back({OpType::Put, key, static_cast<int>(i)});
            } else {
                // Erase 操作
                trace.push_back({OpType::Erase, key, 0});
            }
        }
    }

    return trace;
}

/**
 * Workload B: Scan + Hot (扫描干扰)
 *
 * 逻辑说明：
 * 1. 扫描器 (Scanner): 从 key 0 开始，每次 +1，循环遍历 [0, key_space)。
 * 2. 节奏控制 (scan_batch_size): 每连续进行 N 次扫描读取，插入 1 次热点读取。
 * 3. 热点选择 (Hotspot): 从 [0, hotset_size) 中随机选择一个 key。
 * 4. 写操作混合 (Write Injection): 根据 read_ratio，在生成的读操作中按概率替换为 Put/Erase。
 *
 * 这个 workload 的价值：
 * - 连续扫描会污染“只看最近访问”的缓存策略（例如 LRU）。
 * - 热点访问则要求缓存能尽量保住真正重要的数据。
 */
Trace createScanHotTrace(size_t ops, size_t key_space, size_t hotset_size,
                         size_t scan_batch_size, double read_ratio, unsigned int seed) {
    Trace trace;
    trace.reserve(ops);

    if (ops == 0 || key_space == 0) return trace;
    if (hotset_size > key_space) hotset_size = key_space;
    if (scan_batch_size == 0) scan_batch_size = 1; // 避免除零或死循环

    size_t current_scan_key = 0; // 扫描指针
    size_t op_count = 0;

    // 用于写操作的计数器
    size_t write_counter = 0;

    while (op_count < ops) {
        // --- 阶段 1: 执行一批扫描操作 ---
        for (size_t i = 0; i < scan_batch_size && op_count < ops; ++i) {
            int key = static_cast<int>(current_scan_key);

            // 决定是否将此读操作转换为写操作
            double r = static_cast<double>(rand_r(&seed)) / RAND_MAX;
            if (r < read_ratio) {
                // Read (Scan)
                trace.push_back({OpType::Get, key, 0});
            } else {
                // Write (Scan target)
                // 写操作分布: 90% Put, 10% Erase
                double w_type = static_cast<double>(rand_r(&seed)) / RAND_MAX;
                if (w_type < 0.9) {
                    trace.push_back({OpType::Put, key, static_cast<int>(write_counter++)});
                } else {
                    trace.push_back({OpType::Erase, key, 0});
                }
            }

            op_count++;
            // 扫描指针前进，循环回绕
            current_scan_key = (current_scan_key + 1) % key_space;
        }

        // --- 阶段 2: 插入一次热点访问 (如果还没达到总操作数) ---
        if (op_count >= ops) break;
        if (hotset_size == 0) {
            continue;
        }

        // 从热点集合中选一个 key
        int hot_key = rand_r(&seed) % static_cast<int>(hotset_size);

        // 同样应用 read_ratio 逻辑 (虽然热点通常是读，但为了模拟真实场景，也可能更新热点)
        double r_hot = static_cast<double>(rand_r(&seed)) / RAND_MAX;
        if (r_hot < read_ratio) {
            // Read (Hot)
            trace.push_back({OpType::Get, hot_key, 0});
        } else {
            // Write (Hot)
            double w_type = static_cast<double>(rand_r(&seed)) / RAND_MAX;
            if (w_type < 0.9) {
                trace.push_back({OpType::Put, hot_key, static_cast<int>(write_counter++)});
            } else {
                trace.push_back({OpType::Erase, hot_key, 0});
            }
        }

        op_count++;
    }

    return trace;
}

// Workload C：Random
// - key 完全随机
// - 操作类型也随机（按 read_ratio 与写入内部比例控制）
// - 用作“弱局部性 / 基线场景”
Trace createRandomTrace(size_t ops, size_t key_space, double read_ratio, unsigned int seed) {
    Trace trace;
    trace.reserve(ops);

    if (ops == 0 || key_space == 0) return trace;

    for (size_t i = 0; i < ops; ++i) {
        const double r = static_cast<double>(rand_r(&seed)) / RAND_MAX;
        const int key = rand_r(&seed) % static_cast<int>(key_space);

        if (r < read_ratio) {
            trace.push_back({OpType::Get, key, 0});
        } else {
            const double w_type = static_cast<double>(rand_r(&seed)) / RAND_MAX;
            if (w_type < 0.9) {
                trace.push_back({OpType::Put, key, static_cast<int>(i)});
            } else {
                trace.push_back({OpType::Erase, key, 0});
            }
        }
    }

    return trace;
}

struct BenchmarkConfig {
    size_t capacity;
    size_t key_space;
    size_t hotset_size;
    size_t ops;
    double read_ratio;
    size_t rounds;
    size_t warmup_rounds;
};

// 单次 benchmark 的原始结果。
// 这里保留尽可能多的“原始计数”，便于后续继续扩展更多统计指标。
struct BenchmarkResult {
    std::string cache_name;
    std::string workload_name;
    size_t total_ops = 0;
    size_t get_ops = 0;
    size_t put_ops = 0;
    size_t erase_ops = 0;
    size_t hits = 0;
    size_t misses = 0;
    double elapsed_ms = 0.0;

    [[nodiscard]] double hit_rate() const {
        if (get_ops == 0) return 0.0;
        return static_cast<double>(hits) / static_cast<double>(get_ops);
    }

    [[nodiscard]] double throughput_ops_per_sec() const {
        if (elapsed_ms <= 0.0) return 0.0;
        return static_cast<double>(total_ops) * 1000.0 / elapsed_ms;
    }
};

// 多轮运行后的汇总结果。
// 这里先做最实用的一版：平均值 + 最小值 + 最大值。
struct BenchmarkSummary {
    std::string cache_name;
    std::string workload_name;
    size_t total_ops = 0;
    size_t get_ops = 0;
    size_t put_ops = 0;
    size_t erase_ops = 0;
    double avg_hit_rate = 0.0;
    double min_hit_rate = std::numeric_limits<double>::max();
    double max_hit_rate = std::numeric_limits<double>::lowest();
    double avg_elapsed_ms = 0.0;
    double min_elapsed_ms = std::numeric_limits<double>::max();
    double max_elapsed_ms = std::numeric_limits<double>::lowest();
    double avg_throughput = 0.0;
    double min_throughput = std::numeric_limits<double>::max();
    double max_throughput = std::numeric_limits<double>::lowest();
};

struct WorkloadCase {
    std::string name;
    Trace trace;
};

struct CacheCase {
    std::string name;
};

BenchmarkResult runBenchmark(const std::string &cache_name,
                             mycache::ICache<int, int> &cache,
                             const Trace &trace,
                             const std::string &workload_name) {
    BenchmarkResult result;
    result.cache_name = cache_name;
    result.workload_name = workload_name;
    result.total_ops = trace.size();

    const auto begin = std::chrono::steady_clock::now();

    for (const auto &entry: trace) {
        switch (entry.type) {
            case OpType::Get: {
                ++result.get_ops;
                auto v = cache.get(entry.key);
                if (v.has_value()) {
                    ++result.hits;
                } else {
                    ++result.misses;
                }
                break;
            }
            case OpType::Put: {
                ++result.put_ops;
                cache.put(entry.key, entry.value);
                break;
            }
            case OpType::Erase: {
                ++result.erase_ops;
                cache.erase(entry.key);
                break;
            }
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - begin).count();
    return result;
}

BenchmarkSummary summarizeResults(const std::vector<BenchmarkResult> &results) {
    BenchmarkSummary summary;
    if (results.empty()) return summary;

    summary.cache_name = results.front().cache_name;
    summary.workload_name = results.front().workload_name;
    summary.total_ops = results.front().total_ops;
    summary.get_ops = results.front().get_ops;
    summary.put_ops = results.front().put_ops;
    summary.erase_ops = results.front().erase_ops;

    for (const auto &result: results) {
        const double hit_rate = result.hit_rate();
        const double throughput = result.throughput_ops_per_sec();

        summary.avg_hit_rate += hit_rate;
        summary.min_hit_rate = std::min(summary.min_hit_rate, hit_rate);
        summary.max_hit_rate = std::max(summary.max_hit_rate, hit_rate);

        summary.avg_elapsed_ms += result.elapsed_ms;
        summary.min_elapsed_ms = std::min(summary.min_elapsed_ms, result.elapsed_ms);
        summary.max_elapsed_ms = std::max(summary.max_elapsed_ms, result.elapsed_ms);

        summary.avg_throughput += throughput;
        summary.min_throughput = std::min(summary.min_throughput, throughput);
        summary.max_throughput = std::max(summary.max_throughput, throughput);
    }

    const auto n = static_cast<double>(results.size());
    summary.avg_hit_rate /= n;
    summary.avg_elapsed_ms /= n;
    summary.avg_throughput /= n;
    return summary;
}

std::unique_ptr<mycache::ICache<int, int>> makeCache(const std::string &cache_name, size_t capacity) {
    using namespace mycache;

    if (cache_name == "LRU") {
        return std::make_unique<LRUCache<int, int>>(capacity);
    }
    if (cache_name == "LFU") {
        return std::make_unique<LFUCache<int, int>>(capacity);
    }
    if (cache_name == "ARC") {
        return std::make_unique<ARCCache<int, int>>(capacity);
    }
    return nullptr;
}

BenchmarkSummary runBenchmarkCase(const CacheCase &cache_case,
                                  const WorkloadCase &workload_case,
                                  const BenchmarkConfig &config) {
    std::vector<BenchmarkResult> measured_results;
    measured_results.reserve(config.rounds);

    for (size_t i = 0; i < config.warmup_rounds; ++i) {
        auto cache = makeCache(cache_case.name, config.capacity);
        if (cache) {
            (void) runBenchmark(cache_case.name, *cache, workload_case.trace, workload_case.name);
        }
    }

    for (size_t i = 0; i < config.rounds; ++i) {
        auto cache = makeCache(cache_case.name, config.capacity);
        if (cache) {
            measured_results.push_back(runBenchmark(cache_case.name, *cache, workload_case.trace, workload_case.name));
        }
    }

    return summarizeResults(measured_results);
}

void printSummaryHeader() {
    std::cout << std::left
              << std::setw(10) << "Cache"
              << std::setw(14) << "Workload"
              << std::setw(10) << "TotalOps"
              << std::setw(10) << "Gets"
              << std::setw(10) << "Puts"
              << std::setw(10) << "Erases"
              << std::setw(14) << "Hit(avg)"
              << std::setw(22) << "Hit(min~max)"
              << std::setw(14) << "TimeAvg(ms)"
              << std::setw(18) << "ThrAvg(op/s)"
              << std::setw(24) << "Thr(min~max)"
              << '\n';
    std::cout << std::string(156, '-') << '\n';
}

std::string formatRange(double min_v, double max_v, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << min_v << "~" << max_v;
    return oss.str();
}

void printSummaryRow(const BenchmarkSummary &summary) {
    std::cout << std::left
              << std::setw(10) << summary.cache_name
              << std::setw(14) << summary.workload_name
              << std::setw(10) << summary.total_ops
              << std::setw(10) << summary.get_ops
              << std::setw(10) << summary.put_ops
              << std::setw(10) << summary.erase_ops
              << std::setw(14) << std::fixed << std::setprecision(4) << summary.avg_hit_rate
              << std::setw(22) << formatRange(summary.min_hit_rate, summary.max_hit_rate, 4)
              << std::setw(14) << std::fixed << std::setprecision(3) << summary.avg_elapsed_ms
              << std::setw(18) << std::fixed << std::setprecision(2) << summary.avg_throughput
              << std::setw(24) << formatRange(summary.min_throughput, summary.max_throughput, 2)
              << '\n';
}

int main(int argc, char **argv) {
    using namespace mycache;
    (void) argc;
    (void) argv;

    // 这组参数先追求“容易理解、运行快、结果有区分度”，而不是极限严谨的论文级 benchmark。
    const BenchmarkConfig config{
            .capacity = 128,
            .key_space = 1024,
            .hotset_size = 64,
            .ops = 200000,
            .read_ratio = 0.90,
            .rounds = 5,
            .warmup_rounds = 1,
    };

    const unsigned int seed = 20260311U;
    std::vector<WorkloadCase> workloads;
    workloads.reserve(3);
    workloads.push_back({"HotSet", createHotSetTrace(config.ops, config.key_space, config.hotset_size,
                                                     0.80, config.read_ratio, seed)});
    workloads.push_back({"ScanHot", createScanHotTrace(config.ops, config.key_space, config.hotset_size,
                                                       16, config.read_ratio, seed + 1U)});
    workloads.push_back({"Random", createRandomTrace(config.ops, config.key_space,
                                                     config.read_ratio, seed + 2U)});

    const std::vector<CacheCase> caches{{"LRU"}, {"LFU"}, {"ARC"}};

    std::vector<BenchmarkSummary> summaries;
    summaries.reserve(workloads.size() * caches.size());

    for (const auto &workload: workloads) {
        for (const auto &cache: caches) {
            summaries.push_back(runBenchmarkCase(cache, workload, config));
        }
    }

    std::cout << "Benchmark config: "
              << "capacity=" << config.capacity
              << ", key_space=" << config.key_space
              << ", hotset_size=" << config.hotset_size
              << ", ops=" << config.ops
              << ", read_ratio=" << config.read_ratio
              << ", warmup_rounds=" << config.warmup_rounds
              << ", measured_rounds=" << config.rounds
              << '\n';
    std::cout << "Note: hit rate only counts Get operations; throughput counts all operations." << '\n' << '\n';

    printSummaryHeader();
    for (const auto &summary: summaries) {
        printSummaryRow(summary);
    }

    return 0;
}