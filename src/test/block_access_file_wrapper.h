// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BERRYDB_TEST_BLOCK_ACCESS_FILE_WRAPPER_H_
#define BERRYDB_TEST_BLOCK_ACCESS_FILE_WRAPPER_H_

#include "berrydb/vfs.h"

namespace berrydb {

/** A wrapper for BlockAccessFile for testing.
 *
 * The wrapper forwards I/O calls to the underlying BlockAccessFile until
 * SetAccessError() is called. Afterwards, I/O calls are not forwarded, and
 * instead immediately return the status given to SetAccessError(). */
class BlockAccessFileWrapper : public BlockAccessFile {
 public:
  /** Creates a wrapper for a file. */
  BlockAccessFileWrapper(BlockAccessFile* file);
  ~BlockAccessFileWrapper();

  /** Inject errors in I/O calls.
   *
   * @param access_error the error status that will be returned by I/O calls;
   *                     passing in kSuccess stops the error injection behavior
   */
  inline void SetAccessError(Status access_error) noexcept {
    access_error_ = access_error;
  }

  // BlockAccessFile API.
  Status Read(size_t offset, size_t byte_count, uint8_t* buffer) override;
  Status Write(uint8_t* buffer, size_t offset, size_t byte_count) override;
  Status Sync() override;
  Status Close() override;

 private:
  BlockAccessFile* const file_;
  Status access_error_;
  bool is_closed_ = false;
};

}  // namespace berrydb

#endif  // BERRYDB_TEST_BLOCK_ACCESS_FILE_WRAPPER_H_
