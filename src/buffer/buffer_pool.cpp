#include "buffer_pool.h"
#include <iostream>
#include <random>
#include <cstring>
#include <cassert>
namespace Lemon {
Page::Page() :
    space_id_(),
    page_id_(),
    lsn_(),
    data_(new unsigned char[DATA_PAGE_SIZE]),
    state_(State::INVALID) {

}

Page::~Page() {
  if (data_ != nullptr) {
    delete[] data_;
    data_ = nullptr;
  }
}

BufferPool::BufferPool() :
    lru_list_(),
    hash_map_(),
    buffer_(new Page[BUFFER_POOL_SIZE]),
    data_path_("/home/lemon/mysql/data/"),
    space_id_2_file_name_(),
    free_list_(), frame_id_2_page_address_(BUFFER_POOL_SIZE) {

  // 1. 构建映射表
  std::vector<std::string> filenames;
  TravelDirectory(data_path_, ".ibd", filenames);
  std::ifstream ifs;
  unsigned char page_buf[DATA_PAGE_SIZE];
  for (auto & filename : filenames) {
    ifs.open(filename, std::ios::in | std::ios::binary);
    ifs.read(reinterpret_cast<char *>(page_buf), DATA_PAGE_SIZE);
    uint32_t space_id = mach_read_from_4(page_buf + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
    space_id_2_file_name_.insert({space_id, PageReader(filename)});
    std::cout << space_id << "->" << filename << std::endl;
    ifs.close();
  }

  // 2. 初始化free_list_
  for (int i = 0; i < static_cast<int>(BUFFER_POOL_SIZE); ++i) {
    free_list_.emplace_back(i);
  }
}


BufferPool::~BufferPool() {
  if (buffer_ != nullptr) {
    delete[] buffer_;
    buffer_ = nullptr;
  }
}

Page *BufferPool::NewPage(space_id_t space_id, page_id_t page_id) {
  if (hash_map_.find(space_id) != hash_map_.end()
      && hash_map_[space_id].find(page_id) != hash_map_[space_id].end()) {
    std::cerr << "the page(space_id = " << space_id
              << ", page_id = " << page_id << ") was already in buffer pool"
              << std::endl;
    return nullptr;
  }
  if (free_list_.empty()) {
    // buffer pool 空间不够
    Evict(64);
  }
  // 从free list申请一个buffer frame
  frame_id_t frame_id = free_list_.front();
  free_list_.pop_front();

  assert(frame_id_2_page_address_[frame_id].in_lru_ == false);

  // 初始化申请到的buffer frame
  buffer_[frame_id].Reset();
  buffer_[frame_id].SetSpaceId(space_id);
  buffer_[frame_id].SetPageId(page_id);
  buffer_[frame_id].SetState(Page::State::FROM_BUFFER);

  // 新创建的page加入lru list
  lru_list_.emplace_front(frame_id);
  hash_map_[space_id][page_id] = lru_list_.begin();

  // 初始化frame_id_2_page_address_
  frame_id_2_page_address_[frame_id].in_lru_ = true;
  frame_id_2_page_address_[frame_id].space_id_ = space_id;
  frame_id_2_page_address_[frame_id].page_id_ = page_id;

  return &buffer_[frame_id];
}

void BufferPool::Evict(int n) {
  for (int i = 0; i < n; ++i) {
    if (lru_list_.empty()) return;
    // 把 buffer frame 从 LRU List 中移除
    frame_id_t frame_id = lru_list_.back();
    lru_list_.pop_back();
    assert(frame_id_2_page_address_[frame_id].in_lru_ == true);
    frame_id_2_page_address_[frame_id].in_lru_ = false;
    space_id_t space_id = frame_id_2_page_address_[frame_id].space_id_;
    page_id_t page_id = frame_id_2_page_address_[frame_id].page_id_;
    hash_map_[space_id].erase(page_id);

    // 把 buffer frame 归还 Free List
    free_list_.push_back(frame_id);

    buffer_[frame_id].SetState(Page::State::INVALID);
  }
}

Page *BufferPool::GetPage(space_id_t space_id, page_id_t page_id) {
  if (space_id_2_file_name_.find(space_id) == space_id_2_file_name_.end()) {
    std::cerr << "invalid space_id(" << space_id << ")" << std::endl;
    return nullptr;
  }

  // 该 page 已经被lru缓存了
  if (hash_map_.find(space_id) != hash_map_.end()
      && hash_map_[space_id].find(page_id) != hash_map_[space_id].end()) {
    auto iter = hash_map_[space_id][page_id];
    auto frame_id = *hash_map_[space_id][page_id];

    // 提升到lru list的队头
    lru_list_.erase(iter);
    lru_list_.emplace_front(frame_id);
    hash_map_[space_id][page_id] = lru_list_.begin();

    return &buffer_[frame_id];
  }

  // 不在buffer pool中，从磁盘读
  // TODO 假定所有的Page在磁盘上都是存在的
  return ReadPageFromDisk(space_id, page_id);
}

Page *BufferPool::ReadPageFromDisk(space_id_t space_id, page_id_t page_id) {

  if (free_list_.empty()) {
    // buffer pool 空间不够
    Evict(64);
  }

  // 从free list中分配一个frame，从磁盘读取page，填充这个frame
  frame_id_t frame_id = free_list_.front();
  std::ifstream &ifs = space_id_2_file_name_[space_id].stream_;
  ifs.seekg(static_cast<std::streamoff>(page_id * DATA_PAGE_SIZE));
  ifs.read(reinterpret_cast<char *>(buffer_[frame_id].GetData()), DATA_PAGE_SIZE);
  buffer_[frame_id].SetLSN();
  buffer_[frame_id].SetPageId();
  buffer_[frame_id].SetSpaceId();
  buffer_[frame_id].SetState(Page::State::FROM_DISK);
  free_list_.pop_front();

  assert(frame_id_2_page_address_[frame_id].in_lru_ == false);

  // 将page放入lru list
  frame_id_2_page_address_[frame_id].space_id_ = space_id;
  frame_id_2_page_address_[frame_id].page_id_ = page_id;
  frame_id_2_page_address_[frame_id].in_lru_ = true;
  lru_list_.emplace_front(frame_id);
  hash_map_[space_id][page_id] = lru_list_.begin();
  return &buffer_[frame_id];
}

BufferPool buffer_pool;
}
