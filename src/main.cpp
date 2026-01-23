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
    return 0;
}
