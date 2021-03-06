# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileSystem
from future import Future
import appengine_memcache as memcache

class MemcacheFileSystem(FileSystem):
  """FileSystem implementation which memcaches the results of Read.
  """
  def __init__(self, file_system, memcache):
    self._file_system = file_system
    self._memcache = memcache

  def Stat(self, path):
    """Stats the directory given, or if a file is given, the file's parent
    directory.
    """
    version = self._memcache.Get(path, memcache.MEMCACHE_FILE_SYSTEM_STAT)
    if version is None:
      stat_info = self._file_system.Stat(path)
      self._memcache.Set(path,
                         stat_info.version,
                         memcache.MEMCACHE_FILE_SYSTEM_STAT)
    else:
      stat_info = self.StatInfo(version)
    return stat_info

  def Read(self, paths):
    """Reads a list of files. If a file is in memcache and it is not out of
    date, it is returned. Otherwise, the file is retrieved from the file system.
    """
    result = {}
    uncached = []
    for path in paths:
      cached_result = self._memcache.Get(path,
                                         memcache.MEMCACHE_FILE_SYSTEM_READ)
      if cached_result is None:
        uncached.append(path)
        continue
      data, version = cached_result
      if self.Stat(path).version > version:
        self._memcache.Delete(path, memcache.MEMCACHE_FILE_SYSTEM_READ)
        uncached.append(path)
        continue
      result[path] = data
    new_items = self._file_system.Read(uncached).Get()
    for item in new_items:
      version = self.Stat(item).version
      value = new_items[item]
      self._memcache.Set(item,
                         (value, version),
                         memcache.MEMCACHE_FILE_SYSTEM_READ)
      result[item] = value
    return Future(value=result)
