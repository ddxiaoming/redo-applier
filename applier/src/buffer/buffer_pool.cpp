#include "applier/buffer_pool.h"
#include "applier/logger.h"
#include "applier/file_io.h"
#include "applier/my_assert.h"
namespace Lemon {

Page::Page() :
    data_(new unsigned char[DATA_PAGE_SIZE]),
    state_(State::INVALID) {

}

Page::~Page() {
  if (data_ != nullptr) {
    delete[] data_;
    data_ = nullptr;
  }
}

void Page::WriteCheckSum(uint32_t checksum) {
  mach_write_to_4(data_ + FIL_PAGE_SPACE_OR_CHKSUM, checksum);
  mach_write_to_4(data_ + DATA_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM, checksum);
}

Page::Page(const Page &other) :
  data_(new unsigned char[DATA_PAGE_SIZE]),
  state_(other.state_) {

  memcpy(data_, other.data_, DATA_PAGE_SIZE);
}

BufferPool::BufferPool() {

  // 1. 构建映射表
  space_id_2_start_lpa_.insert(26, (DATA_PAGE_PARTITION0 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(27, (DATA_PAGE_PARTITION1 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(28, (DATA_PAGE_PARTITION2 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(29, (DATA_PAGE_PARTITION3 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(30, (DATA_PAGE_PARTITION4 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(31, (DATA_PAGE_PARTITION5 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(32, (DATA_PAGE_PARTITION6 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(33, (DATA_PAGE_PARTITION7 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(34, (DATA_PAGE_PARTITION8 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(35, (DATA_PAGE_PARTITION9 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(36, (DATA_PAGE_PARTITION10 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(37, (DATA_PAGE_PARTITION11 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(38, (DATA_PAGE_PARTITION12 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(39, (DATA_PAGE_PARTITION13 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(40, (DATA_PAGE_PARTITION14 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(41, (DATA_PAGE_PARTITION15 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(42, (DATA_PAGE_PARTITION16 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(43, (DATA_PAGE_PARTITION17 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(44, (DATA_PAGE_PARTITION18 * PARTITION_SIZE) / FLASH_PAGE_SIZE);
  space_id_2_start_lpa_.insert(45, (DATA_PAGE_PARTITION19 * PARTITION_SIZE) / FLASH_PAGE_SIZE);


  // 2. 初始化free_list_
  for (int i = 0; i < static_cast<int>(BUFFER_POOL_SIZE); ++i) {
    free_list_.emplace_back(i);
  }

  // 3. 初始化frame_id_2_page_address_
  frame_id_2_page_address_.resize(BUFFER_POOL_SIZE/ DATA_PAGE_SIZE);

  LOG_DEBUG("Initialized buffer pool.\n");
}


BufferPool::~BufferPool() {
  if (buffer_ != nullptr) {
    munmap(buffer_, BUFFER_POOL_SIZE);
    buffer_ = nullptr;
  }
}

Page *BufferPool::NewPage(space_id_t space_id, page_id_t page_id) {
  if (hash_map_.find(space_id) != hash_map_.end()
      && hash_map_[space_id].find(page_id) != hash_map_[space_id].end()) {
    LOG_DEBUG("the page(space_id = %d, page_id = %d) was already in buffer pool\n", space_id, page_id);
    return nullptr;
  }
  if (free_list_.empty()) {
    // buffer pool 空间不够
    Evict(64);
  }
  // 从free list申请一个buffer frame
  frame_id_t frame_id = free_list_.front();
  free_list_.pop_front();

  MY_ASSERT(frame_id_2_page_address_[frame_id].in_lru_ == false);

  // 初始化申请到的buffer frame
  buffer_[frame_id].Reset();
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
    space_id_t space_id = frame_id_2_page_address_[frame_id].space_id_;
    page_id_t page_id = frame_id_2_page_address_[frame_id].page_id_;

    // 写回
    WriteBack(space_id, page_id);
    lru_list_.pop_back();
    MY_ASSERT(frame_id_2_page_address_[frame_id].in_lru_ == true);
    frame_id_2_page_address_[frame_id].in_lru_ = false;

    hash_map_[space_id].remove(page_id);

    // 把 buffer frame 归还 Free List
    free_list_.emplace_back(frame_id);

    buffer_[frame_id].SetState(Page::State::INVALID);
  }
}

Page *BufferPool::GetPage(space_id_t space_id, page_id_t page_id) {

  if (space_id_2_start_lpa_.find(space_id) == space_id_2_start_lpa_.end()) {
    LOG_DEBUG("invalid space_id(%d)\n", space_id);
    return nullptr;
  }

  // 该 page 已经被lru缓存了
  if (hash_map_.find(space_id) != hash_map_.end()
      && hash_map_[space_id].find(page_id) != hash_map_[space_id].end()) {
    auto iter = hash_map_[space_id][page_id];
    auto frame_id = (*hash_map_[space_id][page_id])->object;

    // 提升到lru list的队头

    lru_list_.erase(iter);
    lru_list_.emplace_front(frame_id);
    hash_map_[space_id][page_id] = lru_list_.begin();

    return &buffer_[frame_id];
  }

  // 不在buffer pool中，从磁盘读
  return ReadPageFromDisk(space_id, page_id);
}

Page *BufferPool::ReadPageFromDisk(space_id_t space_id, page_id_t page_id) {

  if (free_list_.empty()) {
    // buffer pool 空间不够
    Evict(64);
  }

  // 从free list中分配一个frame，从磁盘读取page，填充这个frame
  frame_id_t frame_id = free_list_.front();
  auto start_lpa = space_id_2_start_lpa_[space_id];
  flash_read(page_id + start_lpa, 1, buffer_[frame_id].GetData());
  buffer_[frame_id].SetState(Page::State::FROM_DISK);
  free_list_.pop_front();

  MY_ASSERT(frame_id_2_page_address_[frame_id].in_lru_ == false);

  // 将page放入lru list
  frame_id_2_page_address_[frame_id].space_id_ = space_id;
  frame_id_2_page_address_[frame_id].page_id_ = page_id;
  frame_id_2_page_address_[frame_id].in_lru_ = true;
  lru_list_.emplace_front(frame_id);
  hash_map_[space_id][page_id] = lru_list_.begin();
  return &buffer_[frame_id];
}

bool BufferPool::WriteBack(space_id_t space_id, page_id_t page_id) {
  // 找找看是不是在buffer pool中
  if (hash_map_.find(space_id) != hash_map_.end() && hash_map_[space_id].find(page_id) != hash_map_[space_id].end()) {
    frame_id_t frame_id = (*(hash_map_[space_id][page_id]))->object;
    auto start_lpa = space_id_2_start_lpa_[space_id];
    flash_write(start_lpa + page_id, 1, buffer_[frame_id].GetData());
    return true;
  }

  LOG_DEBUG("page(space_id = %d, page_id = %d) is not in buffer pool. can not write back to ssd.\n", space_id, page_id);
  return false;
}

}
