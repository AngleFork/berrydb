// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./page_pool.h"

#include <cstring>

#include "berrydb/platform.h"
#include "./store_impl.h"

namespace berrydb {

PagePool::PagePool(PoolImpl* pool, size_t page_shift, size_t page_capacity)
    : page_shift_(page_shift), page_size_(1 << page_shift),
      page_capacity_(page_capacity), pool_(pool), free_list_(), lru_list_(),
      log_list_() {
  // The page size should be a power of two.
  DCHECK_EQ(page_size_ & (page_size_ - 1), 0);
}

PagePool::~PagePool() {
  DCHECK_EQ(0, pinned_pages());

  // We cannot use C++11's range-based for loop because the iterator would get
  // invalidated if we release the page it's pointing to.

  for (auto it = free_list_.begin(); it != free_list_.end(); ) {
    Page* page = *it;
    ++it;
    page->Release(this);
  }

  // The LRU list should be empty, unless we crash-close.

  for (auto it = lru_list_.begin(); it != lru_list_.end(); ) {
    Page* page = *it;
    ++it;
    page->Release(this);
  }
}

void PagePool::UnpinStorePage(Page* page) {
  DCHECK(page != nullptr);
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
  DCHECK(page->store() != nullptr);

  page->RemovePin();
  if (page->IsUnpinned())
    lru_list_.push_back(page);
}

void PagePool::UnpinUnassignedPage(Page* page) {
  DCHECK(page != nullptr);
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
  DCHECK(page->store() == nullptr);

  page->RemovePin();
  if (page->IsUnpinned())
    free_list_.push_back(page);
}

void PagePool::UnassignPageFromStore(Page* page) {
  DCHECK(page != nullptr);
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
  DCHECK(page->store() != nullptr);

  if (page->is_dirty()) {
    StoreImpl* store = page->store();
    Status write_status = store->WritePage(page);
    page->MarkDirty(false);

    page->UnassignFromStore();
    if (write_status != Status::kSuccess)
      store->Close();
  } else {
    page->UnassignFromStore();
  }
}

Page* PagePool::AllocPage() {
  if (!free_list_.empty()) {
    // The free list is used as a stack (LIFO), because the last used free page
    // has the highest chance of being in the CPU's caches.
    Page* page = free_list_.front();
    free_list_.pop_front();
    page->AddPin();
    DCHECK(page->store() == nullptr);
    DCHECK(!page->is_dirty());
    return page;
  }

  if (!lru_list_.empty()) {
    Page* page = lru_list_.front();
    lru_list_.pop_front();
    DCHECK_EQ(1,
      page_map_.count(std::make_pair(page->store(), page->page_id())));
    page_map_.erase(std::make_pair(page->store(), page->page_id()));
    page->AddPin();
    UnassignPageFromStore(page);
    return page;
  }

  if (page_count_ < page_capacity_) {
    ++page_count_;
    Page* page = Page::Create(this);
    return page;
  }

  return nullptr;
}

Status PagePool::FetchStorePage(Page *page, PageFetchMode fetch_mode) {
  DCHECK(page != nullptr);
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
  DCHECK(page->store() != nullptr);

  if (fetch_mode == PagePool::kFetchPageData)
    return page->store()->ReadPage(page);

  page->MarkDirty(true);

#if DCHECK_IS_ON()
  // Fill the page with recognizable garbage (as opposed to random garbage), to
  // make it easier to spot code that uses uninitialized page data.
  std::memset(page->data(), 0xCD, page_size_);
#endif  // DCHECK_IS_ON()

  return Status::kSuccess;
}

Status PagePool::AssignPageToStore(
    Page* page, StoreImpl* store, size_t page_id, PageFetchMode fetch_mode) {
  DCHECK(page != nullptr);
  DCHECK(store != nullptr);
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
  DCHECK(page->store() == nullptr);

  page->AssignToStore(store, page_id);
  Status fetch_status = FetchStorePage(page, fetch_mode);
  if (fetch_status == Status::kSuccess) {
    page_map_[std::make_pair(store, page_id)] = page;
    return Status::kSuccess;
  }

  page->UnassignFromStore();
  // Calling UnpinUnassignedPage will perform an extra check compared to
  // inlining the code, because the inlined version would know that the page is
  // unpinned.
  UnpinUnassignedPage(page);
  DCHECK(page->IsUnpinned());
  return fetch_status;
}

Status PagePool::StorePage(
    StoreImpl* store, size_t page_id, PageFetchMode fetch_mode, Page** result) {
  DCHECK(store != nullptr);

  const auto& it = page_map_.find(std::make_pair(store, page_id));
  if (it != page_map_.end()) {
    Page* page = it->second;
    DCHECK_EQ(store, page->store());
    DCHECK_EQ(page_id, page->page_id());
#if DCHECK_IS_ON()
    DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()
    *result = page;
    return Status::kSuccess;
  }

  Page* page = AllocPage();
  if (page == nullptr)
    return Status::kPoolFull;
#if DCHECK_IS_ON()
  DCHECK_EQ(this, page->page_pool());
#endif  // DCHECK_IS_ON()

  Status status = AssignPageToStore(page, store, page_id, fetch_mode);
  if (status == Status::kSuccess)
    *result = page;
  return status;
}

}  // namespace berrydb
