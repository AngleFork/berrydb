// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BERRYDB_STORE_IMPL_H_
#define BERRYDB_STORE_IMPL_H_

#include <functional>
#include <unordered_set>

#include "berrydb/platform.h"
#include "berrydb/pool.h"
#include "berrydb/store.h"
#include "berrydb/vfs.h"
#include "./format/store_header.h"
#include "./page.h"
#include "./util/linked_list.h"

namespace berrydb {

class BlockAccessFile;
class CatalogImpl;
class PagePool;
class PoolImpl;
class TransactionImpl;

/** Internal representation for the Store class in the public API. */
class StoreImpl {
 public:
  /** Create a StoreImpl instance.
   *
   * This returns a minimally set up instance that can be registered with the
   * pool. The new instance should be initalized via StoreImpl::Initialize()
   * before it is used for transactions.
   */
  static StoreImpl* Create(
      BlockAccessFile* data_file, size_t data_file_size,
      RandomAccessFile* log_file, size_t log_file_size, PagePool* page_pool,
      const StoreOptions& options);

  /** Computes the internal representation for a pointer from the public API. */
  static inline StoreImpl* FromApi(Store* api) noexcept {
    StoreImpl* impl = reinterpret_cast<StoreImpl*>(api);
    DCHECK_EQ(api, &impl->api_);
    return impl;
  }
  /** Computes the internal representation for a pointer from the public API. */
  static inline const StoreImpl* FromApi(const Store* api) noexcept {
    const StoreImpl* impl = reinterpret_cast<const StoreImpl*>(api);
    DCHECK_EQ(api, &impl->api_);
    return impl;
  }

  /** Computes the public API representation for this store. */
  inline Store* ToApi() noexcept { return &api_; }

  // See the public API documention for details.
  static std::string LogFilePath(const std::string& store_path);
  TransactionImpl* CreateTransaction();
  inline CatalogImpl* RootCatalog() noexcept { return nullptr; }
  Status Close();
  inline bool IsClosed() const noexcept { return state_ == State::kClosed; }
  void Release();

  /** Initializes a store obtained by Store::Create.
   *
   * Store::Create() gets the store to a state where it can honor the Close()
   * call, so it can be registered with its resource pool. Before the store can
   * process user transactions, it must be initialized using this method.
   *
   * This method writes the initial on-disk data structures for new stores, and
   * kicks off the recovery process for existing stores. Therefore, it is quite
   * possible that initialization will fail due to an I/O error. Callers should
   * be prepared to handle the error.
   */
  Status Initialize(const StoreOptions& options);

  /** Builds a new store on the currently opened files. */
  Status Bootstrap();

  /** Reads a page from the store into the page pool.
   *
   * The page pool entry must have already been assigned to store, and must not
   * be holding onto a dirty page.
   *
   * @param  page the page pool entry that will hold the store's page;
   * @return      most likely kSuccess or kIoError */
  Status ReadPage(Page* page);

  /** Writes a page to the store.
   *
   * @param  page the page pool entry caching the store page to be written
   * @return      most likely kSuccess or kIoError */
  Status WritePage(Page* page);

  /** Updates the store to reflect a transaction's commit / abort.
   *
   * @param transaction must be associated with this store, and closed */
  void TransactionClosed(TransactionImpl* transaction);

  /** Called when a Page is assigned to this store.
   *
   * This method registers the page on the store's list of assigned pages, so
   * the page can be unassigned when the store is closed. */
  inline void PageAssigned(Page* page) noexcept {
    DCHECK(page != nullptr);
    DCHECK_EQ(this, page->store());
#if DCHECK_IS_ON()
    DCHECK_EQ(page_pool_, page->page_pool());
#endif  // DCHECK_IS_ON()

    pool_pages_.push_back(page);
  }

  /** Called when a Page is unassigned from this store.
   *
   * Calls to this method must be paired with PageAssigned() calls. */
  inline void PageUnassigned(Page* page) noexcept {
    DCHECK(page != nullptr);
    DCHECK(page->store() == nullptr);
#if DCHECK_IS_ON()
    DCHECK_EQ(page_pool_, page->page_pool());
#endif  // DCHECK_IS_ON()

    pool_pages_.erase(page);
  }

#if DCHECK_IS_ON()
  /** The page pool used by this store. For use in DCHECKs only. */
  inline PagePool* page_pool() const noexcept { return page_pool_; }
#endif  // DCHECK_IS_ON()

 private:
  /** Use StoreImpl::Create() to obtain StoreImpl instances. */
  StoreImpl(
      BlockAccessFile* data_file, size_t data_file_size,
      RandomAccessFile* log_file, size_t log_file_size, PagePool* page_pool,
      const StoreOptions& options);
  /** Use Release() to destroy StoreImpl instances. */
  ~StoreImpl();

  enum class State {
    kOpen = 0,
    kClosing = 1,
    kClosed = 3,
  };

  /* The public API version of this class. */
  Store api_;  // Must be the first class member.

  /** Handle to the store's data file. */
  BlockAccessFile* const data_file_;

  /** Handle to the store's log file. */
  RandomAccessFile* const log_file_;

  /** The page pool used by this store to interact with its data file. */
  PagePool* const page_pool_;

  /** Metadata in the data file's header. */
  StoreHeader header_;

  /** The transactions opened on this store. */
  LinkedList<TransactionImpl> transactions_;

  /** Pages in the page pool assigned to this store. */
  LinkedList<Page, Page::StoreLinkedListBridge> pool_pages_;

  State state_ = State::kOpen;
};

}  // namespace berrydb

#endif  // BERRYDB_STORE_IMPL_H_
