// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SOURCE_INFO_H_
#define SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SOURCE_INFO_H_

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_payload_map.h"
#include "sync/protocol/sync.pb.h"

namespace base {
class DictionaryValue;
}

namespace syncer {
namespace sessions {

// A container for the source of a sync session. This includes the update
// source, the datatypes triggering the sync session, and possible session
// specific payloads which should be sent to the server.
struct SyncSourceInfo {
  SyncSourceInfo();
  explicit SyncSourceInfo(const ModelTypePayloadMap& t);
  SyncSourceInfo(
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource& u,
      const ModelTypePayloadMap& t);
  ~SyncSourceInfo();

  // Caller takes ownership of the returned dictionary.
  base::DictionaryValue* ToValue() const;

  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source;
  ModelTypePayloadMap types;
};

}  // namespace sessions
}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SESSIONS_SYNC_SOURCE_INFO_H_
