// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Browser;

// The LoginUIService helps track per-profile information for the login UI -
// for example, whether there is login UI currently on-screen.
class LoginUIService : public ProfileKeyedService {
 public:
  // Various UI components implement this API to allow LoginUIService to
  // manipulate their associated login UI.
  class LoginUI {
   public:
    // Invoked when the login UI should be brought to the foreground.
    virtual void FocusUI() = 0;

    // Invoked when the login UI should be closed. This can be invoked if the
    // user takes an action that should display new login UI.
    virtual void CloseUI() = 0;
   protected:
    virtual ~LoginUI() {}
  };

  // Interface for obervers of LoginUIService.
  class Observer {
   public:
    // Called when a new login UI is shown.
    // |ui| The login UI that was just shown. Will never be null.
    virtual void OnLoginUIShown(LoginUI* ui) = 0;

    // Called when a login UI is closed.
    // |ui| The login UI that was just closed; will never be null.
    virtual void OnLoginUIClosed(LoginUI* ui) = 0;

   protected:
    virtual ~Observer() {}
  };

  LoginUIService();
  virtual ~LoginUIService();

  // Gets the currently active login UI, or null if no login UI is active.
  LoginUI* current_login_ui() const {
    return ui_;
  }

  // |observer| The observer to add or remove; cannot be NULL.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets the currently active login UI. It is illegal to call this if there is
  // already login UI visible.
  void SetLoginUI(LoginUI* ui);

  // Called when login UI is closed. If the passed UI is the current login UI,
  // sets current_login_ui() to null.
  void LoginUIClosed(LoginUI* ui);

  // Brings the login UI to the foreground, or if there is no login UI,
  // navigates to the login UI page in the given browser.
  // Virtual for mocking purposes.
  virtual void ShowLoginUI(Browser* browser);

 private:
  // Weak pointer to the currently active login UI, or null if none.
  LoginUI* ui_;

  // List of observers.
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(LoginUIService);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_SERVICE_H_
