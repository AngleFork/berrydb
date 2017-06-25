// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BERRYDB_PAGE_H_
#define BERRYDB_PAGE_H_

#include <cstddef>
#include <cstdint>

#include "berrydb/platform.h"
#include "./util/linked_list.h"

namespace berrydb {

class PagePool;
class StoreImpl;
class TransactionImpl;

/** Control block for a page pool entry, which caches a store page.
 *
 * Although this class represents a page pool entry, it is simply named Page,
 * because most of the system only cares about the store page cached into the
 * entry's buffer.
 *
 * Each entry in a page pool has a control block (the members of this class),
 * which is laid out in memory right before the buffer that holds the content of
 * of the cached store page.
 *
 * An entry belongs to the same PagePool for its entire lifetime. The entry's
 * control block does not hold a reference to the pool (in release mode) to save
 * memory.
 *
 * Each page pool entry has a pin count, which works like a reference count.
 * While an entry is pinned (has at least one pin), it will not be evicted.
 * Conversely, unpinned entries may be evicted and assigned to cache different
 * store pages at any time.
 *
 * Most pages will be stored in a doubly linked list used to implement the LRU
 * eviction policy. To reduce memory allocations, the list nodes are embedded in
 * the page control block.
 *
 * Each linked list has a sentinel. For simplicity, the sentinel is simply a
 * page control block without a page data buffer.
 */
class Page {
  enum class Status;

 public:
  /** Allocates an entry that will belong to the given page pool.
   *
   * The returned page has one pin on it, which is owned by the caller. */
  static Page* Create(PagePool* page_pool);

  /** Releases the memory resources used up by this page pool entry.
   *
   * This method invalidates the Page instance, so it must not be used
   * afterwards. */
  void Release(PagePool* page_pool);

  /** The transaction that this page pool entry is assigned to.
   *
   * Each page pool entry that has been modified by an uncommitted transaction
   * is assigned to that transaction. Other page pool entries that cache a
   * store's pages are assigned to the store's init transaction. This
   * relationship is well defined because, according to the concurrency model,
   * no two concurrent transactions may modify the same Space, and each page
   * belongs at most one space.
   *
   * When DCHECKs are enabled, this is null when the page is not assigned to a
   * transaction. When DCHECKs are disabled, the value is undefined when the
   * page is not assigend to a transaction.
   */
  inline TransactionImpl* transaction() const noexcept { return transaction_; }

  /** The page ID of the store page whose data is cached by this pool page.
   *
   * This is undefined if the page pool entry isn't storing a store page's data.
   */
  inline size_t page_id() const noexcept {
    DCHECK(transaction_ != nullptr);
    return page_id_;
  }

  /** True if the page's data was modified since the page was read.
   *
   * This should only be true for pool pages that cache store pages. When a
   * dirty page is removed from the pool, its content must be written to disk.
   */
  inline bool is_dirty() const noexcept {
    DCHECK(!is_dirty_ || transaction_ != nullptr);
    return is_dirty_;
  }

  /** The page data held by this page. */
  inline uint8_t* data() noexcept {
    return reinterpret_cast<uint8_t*>(this + 1);
  }

#if DCHECK_IS_ON()
  /** The pool that this page belongs to. Solely intended for use in DCHECKs. */
  inline const PagePool* page_pool() const noexcept { return page_pool_; }
#endif  // DCHECK_IS_ON

  /** True if the pool page's contents can be replaced. */
  inline bool IsUnpinned() const noexcept { return pin_count_ == 0; }

  /** Increments the page's pin count. */
  inline void AddPin() noexcept {
#if DCHECK_IS_ON()
    DCHECK_NE(pin_count_, kMaxPinCount);
#endif  // DCHECK_IS_ON
    ++pin_count_;
  }

  /** Decrements the page's pin count. */
  inline void RemovePin() noexcept {
    DCHECK(pin_count_ != 0);
    --pin_count_;
  }

  /** Track the fact that the pool page will cache a store page.
   *
   * The page should not be in any list while a store page is loaded into it,
   * so Alloc() doesn't grab it. This also implies that the page must be pinned.
   *
   * The caller must immediately call TransactionImpl::PageAssigned(). This
   * method cannot make that call, due to header dependencies.
   */
  inline void Assign(TransactionImpl* transaction, size_t page_id) noexcept {
    // NOTE: It'd be nice to DCHECK_EQ(page_pool_, store->page_pool()).
    //       Unfortunately, that requires a dependency on store_impl.h, which
    //       absolutely needs to include page.h.
#if DCHECK_IS_ON()
    DCHECK(transaction_ == nullptr);
    DCHECK(transaction_list_node_.list_sentinel() == nullptr);
    DCHECK(linked_list_node_.list_sentinel() == nullptr);
#endif  // DCHECK_IS_ON()
    DCHECK(pin_count_ != 0);
    DCHECK(!is_dirty_);

    transaction_ = transaction;
    page_id_ = page_id;
  }

  /** Track the fact that the pool page no longer caches a store page.
   *
   * The page must be pinned, as it was caching a store page up until now. This
   * also implies that the page cannot be on any list.
   *
   * The caller must immediately call StoreImpl::PageUnassigned(). This method
   * cannot make that call, due to header dependencies.
   */
  inline void UnassignFromStore() noexcept {
    DCHECK(pin_count_ != 0);
    DCHECK(transaction_ != nullptr);
#if DCHECK_IS_ON()
    DCHECK(transaction_list_node_.list_sentinel() != nullptr);
    DCHECK(linked_list_node_.list_sentinel() == nullptr);
#endif  // DCHECK_IS_ON()

#if DCHECK_IS_ON()
    transaction_ = nullptr;
#endif  // DCHECK_IS_ON()
  }

  /** Changes the page's dirtiness status.
   *
   * The page must be assigned to store while its dirtiness is changed. */
  inline void MarkDirty(bool will_be_dirty = true) noexcept {
    DCHECK(transaction_ != nullptr);
    is_dirty_ = will_be_dirty;
  }

 private:
   /** Use Page::Create() to construct Page instances. */
   Page(PagePool* page);
   ~Page();

#if DCHECK_IS_ON()
  /** The maximum value that pin_count_ can hold.
   *
   * Pages should always be pinned by a very small number of modules.
   * Excessively large pin counts indicate leaks.
   */
  static constexpr size_t kMaxPinCount = ~static_cast<size_t>(0);
#endif  // DCHECK_IS_ON()

  friend class LinkedListBridge<Page>;
  LinkedList<Page>::Node linked_list_node_;

  friend class TransactionLinkedListBridge;
  LinkedList<Page>::Node transaction_list_node_;

  TransactionImpl* transaction_;

  /** The cached page ID, for pool entries that are caching a store's pages.
   *
   * This member's memory is available for use (perhaps via an union) by
   * unassigned pages.
   */
  size_t page_id_;


  /** Number of times the page was pinned. Very similar to a reference count. */
  size_t pin_count_;
  bool is_dirty_ = false;

#if DCHECK_IS_ON()
  PagePool* const page_pool_;
#endif  // DCHECK_IS_ON()

 public:
  /** Bridge for TransactionImpl's LinkedList<Page>.
   *
   * This is public for TransactionImpl's use. */
  class TransactionLinkedListBridge {
   public:
    using Embedder = Page;
    using Node = LinkedListNode<Page>;

    static inline Node* NodeForHost(Embedder* host) noexcept {
      return &host->transaction_list_node_;
    }
    static inline Embedder* HostForNode(Node* node) noexcept {
      Embedder* host = reinterpret_cast<Embedder*>(
          reinterpret_cast<char*>(node) - offsetof(
              Embedder, transaction_list_node_));
      DCHECK_EQ(node, &host->transaction_list_node_);
      return host;
    }
  };
};

}  // namespace berrydb

#endif  // BERRYDB_PAGE_H_
