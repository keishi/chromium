// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_TEST_RESPONSE_GENERATOR_H_
#define WEBKIT_MEDIA_TEST_RESPONSE_GENERATOR_H_

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"

namespace webkit_media {

// Generates WebURLErrors and WebURLResponses suitable for testing purposes.
class TestResponseGenerator {
 public:
  enum Flags {
    kNormal = 0,
    kNoAcceptRanges = 1 << 0,   // Don't include Accept-Ranges in 206 response.
    kNoContentRange = 1 << 1,   // Don't include Content-Range in 206 response.
    kNoContentLength = 1 << 2,  // Don't include Content-Length in 206 response.
    kNoContentRangeInstanceSize = 1 << 3,  // Content-Range: N-M/* in 206.
  };

  // Build an HTTP response generator for the given URL. |content_length| is
  // used to generate Content-Length and Content-Range headers.
  TestResponseGenerator(const GURL& gurl, int64 content_length);

  // Generates a WebURLError object.
  WebKit::WebURLError GenerateError();

  // Generates a regular HTTP 200 response.
  WebKit::WebURLResponse Generate200();

  // Generates a regular HTTP 206 response starting from |first_byte_offset|
  // until the end of the resource.
  WebKit::WebURLResponse Generate206(int64 first_byte_offset);

  // Generates a custom HTTP 206 response starting from |first_byte_offset|
  // until the end of the resource. You can tweak what gets included in the
  // headers via |flags|.
  WebKit::WebURLResponse Generate206(int64 first_byte_offset, Flags flags);

  // Generates a regular HTTP 404 response.
  WebKit::WebURLResponse Generate404();

  // Generates a file:// response starting from |first_byte_offset| until the
  // end of the resource.
  //
  // If |first_byte_offset| is negative a response containing no content length
  // will be returned.
  WebKit::WebURLResponse GenerateFileResponse(int64 first_byte_offset);

  const GURL& gurl() { return gurl_; }
  int64 content_length() { return content_length_; }

 private:
  GURL gurl_;
  int64 content_length_;

  DISALLOW_COPY_AND_ASSIGN(TestResponseGenerator);
};

}  // namespace webkit_media

#endif  // WEBKIT_MEDIA_TEST_RESPONSE_GENERATOR_H_
