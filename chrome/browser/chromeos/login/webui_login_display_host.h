// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

class OobeUI;
class WebUILoginDisplay;
class WebUILoginView;

// WebUI-specific implementation of the OOBE/login screen host. Uses
// WebUILoginDisplay as the login screen UI implementation,
class WebUILoginDisplayHost : public BaseLoginDisplayHost,
                              public content::WebContentsObserver {
 public:
  explicit WebUILoginDisplayHost(const gfx::Rect& background_bounds);
  virtual ~WebUILoginDisplayHost();

  // LoginDisplayHost implementation:
  virtual LoginDisplay* CreateLoginDisplay(
      LoginDisplay::Delegate* delegate) OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual views::Widget* GetWidget() const OVERRIDE;
  virtual void OpenProxySettings() OVERRIDE;
  virtual void SetOobeProgressBarVisible(bool visible) OVERRIDE;
  virtual void SetShutdownButtonEnabled(bool enable) OVERRIDE;
  virtual void SetStatusAreaVisible(bool visible) OVERRIDE;
  virtual void StartWizard(const std::string& first_screen_name,
                           DictionaryValue* screen_parameters) OVERRIDE;
  virtual void StartSignInScreen() OVERRIDE;
  virtual void OnPreferencesChanged() OVERRIDE;

  // BaseLoginDisplayHost overrides:
  virtual WizardController* CreateWizardController() OVERRIDE;
  virtual void OnBrowserCreated() OVERRIDE;

  // Returns instance of the OOBE WebUI.
  OobeUI* GetOobeUI() const;

 protected:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Overridden from content::WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  // Loads given URL. Creates WebUILoginView if needed.
  void LoadURL(const GURL& url);

  // Starts postponed WebUI (OOBE/sign in) if it was waiting for
  // wallpaper animation end.
  void StartPostponedWebUI();

  // Container of the screen we are displaying.
  views::Widget* login_window_;

  // Container of the view we are displaying.
  WebUILoginView* login_view_;

  // Login display we are using.
  WebUILoginDisplay* webui_login_display_;

  // True if the login display is the current screen.
  bool is_showing_login_;

  // True if NOTIFICATION_WALLPAPER_ANIMATION_FINISHED notification has been
  // received.
  bool is_wallpaper_loaded_;

  // True if should not show WebUI on first StartWizard/StartSignInScreen call
  // but wait for wallpaper load animation to finish.
  // Used in OOBE (first boot, boot after update) i.e. till
  // device is marked as registered to postpone loading OOBE screen / sign in.
  bool waiting_for_wallpaper_load_;

  content::NotificationRegistrar registrar_;

  // How many times renderer has crashed.
  int crash_count_;

  // Way to restore if renderer have crashed.
  enum {
    RESTORE_UNKNOWN,
    RESTORE_WIZARD,
    RESTORE_SIGN_IN
  } restore_path_;

  // Stored parameters for StartWizard, required to restore in case of crash.
  std::string wizard_first_screen_name_;
  scoped_ptr<DictionaryValue> wizard_screen_parameters_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginDisplayHost);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_DISPLAY_HOST_H_
