# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from future import Future

class _Response(object):
  def __init__(self):
    self.content = ''
    self.headers = { 'content-type': 'none' }
    self.status_code = 200

class FakeUrlFetcher(object):
  def __init__(self, base_path):
    self._base_path = base_path

  def _ReadFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _ListDir(self, directory):
    files = os.listdir(os.path.join(self._base_path, directory))
    html = '<html><title>Revision: 00000</title>\n'
    for filename in files:
      if os.path.isdir(os.path.join(self._base_path, directory, filename)):
        html += '<a>' + filename + '/</a>\n'
      else:
        html += '<a>' + filename + '</a>\n'
    html += '</html>'
    return html

  def FetchAsync(self, url):
    return Future(value=self.Fetch(url))

  def Fetch(self, url):
    result = _Response()
    if url.endswith('/'):
      result.content = self._ListDir(url)
    else:
      result.content = self._ReadFile(url)
    return result
