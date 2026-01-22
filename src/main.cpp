#include <iostream>
#include "mycache/ICache.h"
#include "mycache/LRUCache.h"

int main() {
    auto *pCache = new mycache::LRUCache<int, int>();
    std::cout <<pCache->put(1, 1)<<" "<<pCache->capacity() << std::endl;
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
