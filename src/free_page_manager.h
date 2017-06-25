// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BERRYDB_FREE_PAGE_MANAGER_H
#define BERRYDB_FREE_PAGE_MANAGER_H

#include "berrydb/platform.h"

namespace berrydb {

class StoreImpl;

/** Tracks the free pages in a store's data file.
 *
 * Each store has a free page manager (a.k.a. free space manager). Pages that
 * become empty after data is deleted cannot be immediately returned to the
 * underlying filesystem, because a store's data file is a continuous sequence
 * of pages. Instead, the page IDs for free pages are stored in a list, so they
 * can be reused. The free page list entries are stored in pages that are
 * exclusively allocated for this purpose.
 */
class FreePageManager {
 public:
  /** Creates a manager for a store's free pages.
   *
   * Intended to be used by the StoreImpl's FreePageManager instance. */
  FreePageManager(StoreImpl* store);
  ~FreePageManager();

  /** Allocates a page and assigns it to a transaction.
   *
   * The page allocation is bound to the given transaction's lifecycle. The page
   * is permanently allocated when the transaction commits. If the transaction
   * is rolled back, the page is returned to the free page list.
   *
   * Under normal circumstances, the allocation should always suceedd. Even if
   * the free list is empty, the store file can be grown. Failure indicates an
   * exceptional circumstance, such as an I/O error or a quota error, so the
   * caller should bail on errors and eventually abort the transaction.
   *
   * @param  transaction the transaction that will use the newly allocated page
   * @return the ID of the allocated page, or kInvalidPageId if the page
   *         allocation fails
   */
  size_t AllocPage(TransactionImpl* transaction);

  /** Queues up a page to be freed when a transaction commits.
   *
   * The free operation is bound to the given transaction's lifecycle. The page
   * is permanently added to the free list when the transaction commits. If the
   * transaction is rolled back, the page is not freed anymore.
   *
   * @param  page_id    the ID of the page that will be freed
   * @param  transation the transaction whose commit will free the page
   * @return            most likely kSuccess or kIoError
   */
  Status FreePage(size_t page_id, TransactionImpl* transaction);

  /** Special AllocPage return value that indicates failure.
   *
   * Zero is a legtimate value because the first page in a store file will
   * always be used for the store's header, so it can never come out of a
   * dynamic allocation.
   */
  static constexpr size_t kInvalidPageId = 0;

 private:
   StoreImpl* store;
};

}  // namespace berrydb

#endif  // BERRYDB_FREE_PAGE_MANAGER_H
