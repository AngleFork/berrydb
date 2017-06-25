// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./free_page_manager.h"

#include "berrydb/platform.h"

namespace berrydb {

FreePageManager::FreePageManager(StoreImpl* store) : store_(store) {
}

FreePageManager::~FreePageManager() {
}

}  // namespace berrydb
