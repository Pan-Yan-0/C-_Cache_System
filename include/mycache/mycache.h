// =============================================================================
// mycache.h (聚合头 / 公共入口)
// =============================================================================
// 目的：
// - 让库的使用者只需要写一行 include：
//     #include "mycache/mycache.h"
//   就能拿到所有对外公开的缓存类型与接口。
//
// 说明：
// - 这里“只做 include 聚合”，不写任何实现逻辑。
// - 该项目目前是模板 header-only 实现，因此包含这些头文件即可使用。
// =============================================================================

#ifndef CACHESTUDY_MYCACHE_MYCACHE_H
#define CACHESTUDY_MYCACHE_MYCACHE_H

#include "mycache/ICache.h"
#include "mycache/LRUCache.h"
#include "mycache/LFUCache.h"

#endif // CACHESTUDY_MYCACHE_MYCACHE_H
