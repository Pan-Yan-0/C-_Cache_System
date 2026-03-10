#include <iostream>
#include <string>
#include <memory>
#include "mycache/ICache.h"
#include "mycache/LRUCache.h"

int main() {
    std::unique_ptr<mycache::LRUCache<int, std::string>> pCache = std::make_unique<mycache::LRUCache<int, std::string>>(
            10);
    std::string st = "string";
    std::cout << pCache->put(1, st) << " " << pCache->capacity() << std::endl;
    pCache->put(2, st);
    pCache->put(3, st);
    std::cout << pCache->get(1).value() << " " << pCache->size() << std::endl;
    std::cout << pCache->erase(1) << " " << pCache->get(1).has_value() << " " << pCache->size() << std::endl;
    std::cout << "Hello, World!" << std::endl;

    // Benchmark harness
    const int numIterations = 100000;
    const int numWarmup = 10000;
    int hitCount = 0;
    int missCount = 0;

    // Warmup
    for (int i = 0; i < numWarmup; ++i) {
        pCache->put(i, st);
    }

    // Benchmark
    for (int i = 0; i < numIterations; ++i) {
        if (pCache->get(i % numWarmup).has_value()) {
            ++hitCount;
        } else {
            ++missCount;
        }
    }

    double hitRate = static_cast<double>(hitCount) / (hitCount + missCount);
    std::cout << "Hit rate: " << hitRate * 100 << "%" << std::endl;

    return 0;
}
