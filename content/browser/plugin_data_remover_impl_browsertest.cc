// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "content/browser/plugin_data_remover_impl.h"
#include "content/public/common/content_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace content {

namespace {
const char* kNPAPITestPluginMimeType = "application/vnd.npapi-test";
}

class PluginDataRemoverTest : public InProcessBrowserTest,
                              public base::WaitableEventWatcher::Delegate {
 public:
  PluginDataRemoverTest() : InProcessBrowserTest() { }

  virtual void OnWaitableEventSignaled(base::WaitableEvent* waitable_event) {
    MessageLoop::current()->Quit();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
#ifdef OS_MACOSX
    FilePath browser_directory;
    PathService::Get(base::DIR_MODULE, &browser_directory);
    command_line->AppendSwitchPath(switches::kExtraPluginDir,
                                   browser_directory.AppendASCII("plugins"));
#endif
    // TODO(jam): since these plugin tests are running under Chrome, we need to
    // tell it to disable its security features for old plugins. Once this is
    // running under content_browsertests, these flags won't be needed.
    // http://crbug.com/90448
    // switches::kAlwaysAuthorizePlugins
    command_line->AppendSwitch("always-authorize-plugins");
  }
};

IN_PROC_BROWSER_TEST_F(PluginDataRemoverTest, RemoveData) {
  PluginDataRemoverImpl plugin_data_remover(GetBrowserContext());
  plugin_data_remover.set_mime_type(kNPAPITestPluginMimeType);
  base::WaitableEventWatcher watcher;
  base::WaitableEvent* event =
      plugin_data_remover.StartRemoving(base::Time());
  watcher.StartWatching(event, this);
  ui_test_utils::RunMessageLoop();
}

}  // namespace content
