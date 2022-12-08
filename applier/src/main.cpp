#include "applier/apply.h"
#include "applier/buffer_pool.h"
#include "stl/hash_map.hpp"
#include "applier/config.h"
#include "stl/list.hpp"
#include "stl/std_compat.hpp"
#include "applier/logger.h"

void apply() {
  LOG_DEBUG("invoking apply function.\n");
//  using LruListType = frg::list<Lemon::frame_id_t, frg::stl_allocator>;
//  LruListType lru_list_{};
//  LOG_DEBUG("initialized lru_list_.\n");
//  // [space_id, page_id] -> iterator 快速定位1个page在 LRU 中的位置
//  using Key1Type = Lemon::space_id_t;
//  using Key2Type = Lemon::page_id_t;
//  using Key2ValueType = frg::hash_map<Key2Type, LruListType::iterator>;
//  frg::hash_map<Key1Type, Key2ValueType> hash_map_{};
//  LOG_DEBUG("initialized hash_map_.\n");
//
//  Lemon::Page *buffer_ {(Lemon::Page *) malloc(Lemon::BUFFER_POOL_SIZE * sizeof(Lemon::Page))};
//  LOG_DEBUG("initialized buffer_.\n");
//
//  // space_id -> 起始LPA的映射表
//  frg::hash_map<uint32_t, uint32_t> space_id_2_start_lpa_ {};
//  LOG_DEBUG("initialized space_id_2_start_lpa_.\n");
//
//  // 指示buffer_中哪个frame是可以用的
//  frg::list<Lemon::frame_id_t, frg::stl_allocator> free_list_ {};
//  LOG_DEBUG("initialized free_list_.\n");
//
//  frg::vector<Lemon::PageAddress> frame_id_2_page_address_ {};
//  LOG_DEBUG("initialized frame_id_2_page_address_.\n");
  Lemon::BufferPool buffer_pool;
  Lemon::ApplySystem apply_system;
  apply_system.SetBufferPool(&buffer_pool);
  while (true) {
    if (!apply_system.PopulateHashMap()) {
      break;
    }
    apply_system.ApplyHashLogs();
  }
}

void test() {

  using ListType = frg::list<int, frg::stl_allocator>;
  using HashMapValueType = frg::hash_map<Lemon::page_id_t, ListType, frg::hash<Lemon::page_id_t>, frg::stl_allocator>;
  frg::hash<Lemon::space_id_t> hasher;
  frg::hash_map<Lemon::space_id_t, HashMapValueType, frg::hash<Lemon::space_id_t>, frg::stl_allocator> map_(hasher);

  map_[1][1].emplace_back(1);
  map_[1][1].emplace_back(2);
  map_[1][1].emplace_back(3);
  map_[1][1].emplace_back(4);
  map_[1][1].emplace_back(5);

  map_[1][2].emplace_back(2);
  map_[1][2].emplace_back(3);
  map_[1][2].emplace_back(4);
  map_[1][2].emplace_back(5);
  map_[1][2].emplace_back(6);

  auto iter = map_.begin();
  auto iter_end = map_.end();
  for (; iter != iter_end; ++iter) {
    spu_printf("key1: %d, ", iter->get<0>());
    auto iter2 = iter->get<1>().begin();
    auto iter2_end = iter->get<1>().end();
    for (; iter2 != iter2_end; ++iter2) {
      spu_printf("key2: %d, ", iter->get<0>());
      auto iter3 = iter2->get<1>().begin();
      auto iter3_end = iter2->get<1>().end();
      spu_printf("list: ");
      for (; iter3 != iter3_end; ++iter3) {
        spu_printf("%d ", *iter3);
      }
      spu_printf("\n");
    }
  }
}