#pragma once
#include <cstdint>
#include <cstring>
#include "applier/utility.h"
#include "stl/list.hpp"
#include "stl/std_compat.hpp"
#include "stl/hash_map.hpp"
#include "stl/vector.hpp"
#include "applier/config.h"
namespace Lemon {

class PageAddress {
public:
  bool in_lru_; // 该page是否在LRU中
  space_id_t space_id_;
  page_id_t page_id_;
};

// 前置声明
class BufferPool;

class Page {
public:
  friend class BufferPool;

  enum class State {
    INVALID = 0, // 刚被初始化，data_还未被填充，不可用
    FROM_BUFFER = 1, // 刚从buffer pool中被创建出来
    FROM_DISK = 2, // 从磁盘中读上来的
  };
  Page();

  // Copy Constructor
  Page(const Page &other);
  ~Page();
  [[nodiscard]] lsn_t GetLSN() const {
    return mach_read_from_8(data_ + FIL_PAGE_LSN);
  }
  [[nodiscard]] uint32_t GetCheckSum() const {
    uint32_t checksum1 = mach_read_from_4(data_ + DATA_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
    uint32_t checksum2 = mach_read_from_4(data_ + FIL_PAGE_SPACE_OR_CHKSUM);
    MY_ASSERT(checksum1 == checksum2);
    return mach_read_from_4(data_ + FIL_PAGE_SPACE_OR_CHKSUM);
  }

  [[nodiscard]] uint32_t GetPageId() const {
    return mach_read_from_4(data_ + FIL_PAGE_OFFSET);
  }
  [[nodiscard]] uint32_t GetSpaceId() const {
    return mach_read_from_4(data_ + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  }
  void Reset() {
    memset(data_, 0, DATA_PAGE_SIZE);
    state_ = State::INVALID;
  }

  // TODO
  void WriteCheckSum(uint32_t checksum);

  [[nodiscard]] unsigned char *GetData() const {
    return data_;
  }
  State GetState() {
    return state_;
  }
  void SetState(State state) {
    state_ = state;
  }

  void WritePageLSN(lsn_t lsn) {
    // 写page的lsn时，有两处地方需要写
    // 1. 头部的FIL_PAGE_LSN属性
    mach_write_to_8(FIL_PAGE_LSN + data_, lsn);
    // 2. 尾部的FIL_PAGE_END_LSN_OLD_CHKSUM属性的后4位
    mach_write_to_8(DATA_PAGE_SIZE
                    - FIL_PAGE_END_LSN_OLD_CHKSUM
                    + data_, lsn);
  }
private:
  byte *data_;
  State state_;
};

class BufferPool {
public:
  BufferPool();
  ~BufferPool();
  // 在buffer pool中新建一个page
  Page *NewPage(space_id_t space_id, page_id_t page_id);

  // 从buffer pool中获取一个page，不存在的话从磁盘获取
  Page *GetPage(space_id_t space_id, page_id_t page_id);
  bool WriteBack(space_id_t space_id, page_id_t page_id);

private:

  using LruListType = frg::list<frame_id_t, frg::stl_allocator>;
  LruListType lru_list_{};

  // [space_id, page_id] -> iterator 快速定位1个page在 LRU 中的位置
  using Key1Type = space_id_t;
  using Key2Type = page_id_t;
  using Key2ValueType = frg::hash_map<Key2Type, LruListType::iterator>;
  frg::hash_map<Key1Type, Key2ValueType> hash_map_ {};


  Page *buffer_ {(Page *) malloc(sizeof(Page) * BUFFER_POOL_SIZE)};

  // space_id -> 起始LPA的映射表
  frg::hash_map<uint32_t, uint32_t> space_id_2_start_lpa_ {};

  // 指示buffer_中哪个frame是可以用的
  frg::list<frame_id_t, frg::stl_allocator> free_list_ {};

  frg::vector<PageAddress> frame_id_2_page_address_ {};
  // 按照LRU规则淘汰一些页面
  void Evict(int n);

  Page *ReadPageFromDisk(space_id_t space_id, page_id_t page_id);


};

}
