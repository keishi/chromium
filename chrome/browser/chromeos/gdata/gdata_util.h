// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "googleurl/src/gurl.h"

class FilePath;
class Profile;

namespace gdata {
namespace util {

// Path constants.

// The extention for dirty files. The file names look like
// "<resource-id>.local".
const char kLocallyModifiedFileExtension[] = "local";
// The extension for mounted files. The file names look like
// "<resource-id>.<md5>.mounted".
const char kMountedArchiveFileExtension[] = "mounted";
const char kWildCard[] = "*";
// The path is used for creating a symlink in "pinned" directory for a file
// which is not yet fetched.
const char kSymLinkToDevNull[] = "/dev/null";

// Returns the GData mount point path, which looks like "/special/gdata".
const FilePath& GetGDataMountPointPath();

// Returns the GData mount path as string.
const std::string& GetGDataMountPointPathAsString();

// Returns the 'local' root of remote file system as "/special".
const FilePath& GetSpecialRemoteRootPath();

// Returns the gdata file resource url formatted as
// chrome://drive/<resource_id>/<file_name>.
GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name);

// Given a profile and a gdata_cache_path, return the file resource url.
void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& gdata_cache_path,
                                GURL* url);

// Returns true if the given path is under the GData mount point.
bool IsUnderGDataMountPoint(const FilePath& path);

// Extracts the GData path from the given path located under the GData mount
// point. Returns an empty path if |path| is not under the GData mount point.
// Examples: ExtractGDatPath("/special/drive/foo.txt") => "drive/foo.txt"
FilePath ExtractGDataPath(const FilePath& path);

// Inserts all possible cache paths for a given vector of paths on gdata mount
// point into the output vector |cache_paths|, and then invokes callback.
// Caller must ensure that |cache_paths| lives until the callback is invoked.
void InsertGDataCachePathsPermissions(
    Profile* profile_,
    scoped_ptr<std::vector<FilePath> > gdata_paths,
    std::vector<std::pair<FilePath, int> >* cache_paths,
    const base::Closure& callback);

// Returns true if gdata is currently active with the specified profile.
bool IsGDataAvailable(Profile* profile);

// Escapes a file name in GData cache.
// Replaces percent ('%'), period ('.') and slash ('/') with %XX (hex)
std::string EscapeCacheFileName(const std::string& filename);

// Unescapes a file path in GData cache.
// This is the inverse of EscapeCacheFileName.
std::string UnescapeCacheFileName(const std::string& filename);

// Extracts resource_id, md5, and extra_extension from cache path.
// Case 1: Pinned and outgoing symlinks only have resource_id.
// Example: path="/user/GCache/v1/pinned/pdf:a1b2" =>
//          resource_id="pdf:a1b2", md5="", extra_extension="";
// Case 2: Normal files have both resource_id and md5.
// Example: path="/user/GCache/v1/tmp/pdf:a1b2.01234567" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="";
// Case 3: Mounted files have all three parts.
// Example: path="/user/GCache/v1/persistent/pdf:a1b2.01234567.mounted" =>
//          resource_id="pdf:a1b2", md5="01234567", extra_extension="mounted".
void ParseCacheFilePath(const FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension);

// Returns true if Drive v2 API is enabled via commandline switch.
bool IsDriveV2ApiEnabled();

// Returns a PlatformFileError that corresponds to the GDataFileError provided.
base::PlatformFileError GDataFileErrorToPlatformError(GDataFileError error);

// Returns true when time string is successfully parsed and output as |time|.
// The time string must be in format yyyy-mm-ddThh:mm:ss.dddTZ (TZ is either
// '+hh:mm', '-hh:mm', 'Z' (representing UTC) or empty string).
bool GetTimeFromString(const base::StringPiece& raw_value, base::Time* time);

}  // namespace util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_UTIL_H_
