#pragma once
#include <cstdint>
#include <unordered_map>
#include <list>
#include "utility.h"
#include <string>
#include <fstream>
#include <vector>
#include <cstring>
#include "config.h"

namespace Lemon {

class PageAddress {
public:
  bool in_lru_; // 该page是否在LRU中
  space_id_t space_id_;
  page_id_t page_id_;
};

class PageReader {
public:
  PageReader() = default;
  explicit PageReader(std::string file_name) :
      file_name_(std::move(file_name)),
      stream_(file_name_, std::ios::in | std::ios::binary) {}
  std::string file_name_;
  std::ifstream stream_;
private:
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
  ~Page();
  lsn_t GetLSN() const {
    return lsn_;
//    return mach_read_from_8(data_ + FIL_PAGE_LSN);
  }
  void Reset() {
    std::memset(data_, 0, DATA_PAGE_SIZE);
    space_id_ = 0;
    page_id_ = 0;
    lsn_ = 0;
    state_ = State::INVALID;
  }
  lsn_t GetSpaceId() const {
    return space_id_;
  }
  lsn_t GetPageId() const {
    return page_id_;
  }
  unsigned char *GetData() const {
    return data_;
  }
  State GetState() {
    return state_;
  }
  void SetState(State state) {
    state_ = state;
  }

  // TODO 写page 的lsn时，有两处地方需要写，
  void WriteLSN(lsn_t lsn) {

  }
private:
  void SetLSN() {
    lsn_ = mach_read_from_8(data_ + FIL_PAGE_LSN);
  }
  void SetLSN(lsn_t lsn) {
    lsn_ = lsn;
  }
  void SetSpaceId() {
    space_id_ = mach_read_from_4(data_ + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
  }
  void SetSpaceId(space_id_t space_id) {
    space_id_ = space_id;
  }
  void SetPageId(page_id_t page_id) {
    page_id_ = page_id;
  }
  void SetPageId() {
    page_id_ = mach_read_from_4(data_ + FIL_PAGE_OFFSET);
  }
  space_id_t space_id_;
  page_id_t page_id_;
  lsn_t lsn_;
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
private:
  std::list<frame_id_t> lru_list_;

  // [space_id, page_id] -> iterator 快速定位1个page在 LRU 中的位置
  std::unordered_map<space_id_t, std::unordered_map<page_id_t, std::list<frame_id_t>::iterator>> hash_map_;
  Page *buffer_;
  // 数据目录的path
  std::string data_path_;
  // space_id -> file name的映射表
  std::unordered_map<uint32_t, PageReader> space_id_2_file_name_;

  // 指示buffer_中哪个frame是可以用的
  std::list<frame_id_t> free_list_;

  std::vector<PageAddress> frame_id_2_page_address_;
  // 随机淘汰一些页面
  void Evict(int n);

  Page *ReadPageFromDisk(space_id_t space_id, page_id_t page_id);
};

extern BufferPool buffer_pool;
}
