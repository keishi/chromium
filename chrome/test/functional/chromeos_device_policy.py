# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional  # Must come before pyauto (and thus, policy_base).
import policy_base


class ChromeosDevicePolicy(policy_base.PolicyTestBase):
  """Tests various ChromeOS device policies."""

  def LoginAsGuest(self):
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Expected to be logged out.')
    policy_base.PolicyTestBase.LoginAsGuest(self)
    self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                    msg='Expected to be logged in.')

  def Login(self, user_index, expect_success):
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Expected to be logged out.')
    policy_base.PolicyTestBase.Login(self,
                                     self._usernames[user_index],
                                     self._passwords[user_index])
    if expect_success:
      self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                      msg='Expected to be logged in.')
    else:
      self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                       msg='Expected to not be logged in.')

  # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
  def TryToDisableLocalStateAutoClearing(self):
    # Try to disable automatic clearing of the local state.
    self.TryToDisableLocalStateAutoClearingOnChromeOS()
    self._local_state_auto_clearing = \
        self.IsLocalStateAutoClearingEnabledOnChromeOS()
    if not self._local_state_auto_clearing:
      # Prevent the inherited Logout() method from cleaning up /home/chronos
      # as this also clears the local state.
      self.set_clear_profile(False)

  def ExtraChromeFlags(self):
    """Sets up Chrome to skip OOBE.

    TODO(bartfab): Ensure OOBE is still skipped when crosbug.com/20709 is fixed.
    Disabling automatic clearing of the local state has the curious side effect
    of removing a flag that disables OOBE. This method adds back the flag.
    """
    flags = policy_base.PolicyTestBase.ExtraChromeFlags(self)
    flags.append('--login-screen=login')
    return flags

  def setUp(self):
    policy_base.PolicyTestBase.setUp(self)
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self._local_state_auto_clearing = \
        self.IsLocalStateAutoClearingEnabledOnChromeOS()

    # Cache user credentials for easy lookup. The first user will become the
    # owner.
    credentials = (self.GetPrivateInfo()['prod_enterprise_test_user'],
                   self.GetPrivateInfo()['prod_enterprise_executive_user'],
                   self.GetPrivateInfo()['prod_enterprise_sales_user'])
    self._usernames = [credential['username'] for credential in credentials]
    self._passwords = [credential['password'] for credential in credentials]

  def tearDown(self):
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    # Try to re-enable automatic clearing of the local state and /home/chronos.
    if not self._local_state_auto_clearing:
      self.TryToEnableLocalStateAutoClearingOnChromeOS()
      self.set_clear_profile(True)
    policy_base.PolicyTestBase.tearDown(self)

  def _CheckGuestModeAvailableInLoginWindow(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               !document.getElementById('guestSignin').hidden);
        """)

  def _CheckGuestModeAvailableInAccountPicker(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               !!document.getElementById('pod-row').getPodWithUsername_(''));
        """)

  def _CheckPodVisible(self, username):
    javascript = """
        var pod = document.getElementById('pod-row').getPodWithUsername_('%s');
        window.domAutomationController.send(!!pod && !pod.hidden);
        """
    return self.ExecuteJavascriptInOOBEWebUI(javascript % username)

  def _WaitForPodVisibility(self, username, visible):
    self.assertTrue(
        self.WaitUntil(function=lambda: self._CheckPodVisible(username),
                       expect_retval=visible),
        msg='Expected pod for user %s to %s be visible.' %
            (username, '' if visible else 'not'))

  def testGuestModeEnabled(self):
    """Checks that guest mode login can be enabled/disabled."""
    self.SetDevicePolicy({'guest_mode_enabled': True})
    self.assertTrue(self._CheckGuestModeAvailableInLoginWindow(),
                    msg='Expected guest mode to be available.')
    self.LoginAsGuest()
    self.Logout()

    self.SetDevicePolicy({'guest_mode_enabled': False})
    self.assertFalse(self._CheckGuestModeAvailableInLoginWindow(),
                     msg='Expected guest mode to not be available.')

    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self.TryToDisableLocalStateAutoClearing()
    if self._local_state_auto_clearing:
      logging.warn("""Unable to disable local state clearing. Skipping remainder
                      of test.""")
      return

    # Log in as a regular so that the pod row contains at least one pod and the
    # account picker is shown.
    self.Login(user_index=0, expect_success=True)
    self.Logout()

    self.SetDevicePolicy({'guest_mode_enabled': True})
    self.assertTrue(self._CheckGuestModeAvailableInAccountPicker(),
                    msg='Expected guest mode to be available.')
    self.LoginAsGuest()
    self.Logout()

    self.SetDevicePolicy({'guest_mode_enabled': False})
    self.assertFalse(self._CheckGuestModeAvailableInAccountPicker(),
                     msg='Expected guest mode to not be available.')

  def testShowUserNamesOnSignin(self):
    """Checks that the account picker can be enabled/disabled."""
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self.TryToDisableLocalStateAutoClearing()
    if self._local_state_auto_clearing:
      logging.warn('Unable to disable local state clearing. Skipping test.')
      return

    # Log in as a regular user so that the pod row contains at least one pod and
    # the account picker can be shown.
    self.Login(user_index=0, expect_success=True)
    self.Logout()

    self.SetDevicePolicy({'show_user_names': False})
    self._WaitForLoginScreenId('gaia-signin')

    self.SetDevicePolicy({'show_user_names': True})
    self._WaitForLoginScreenId('account-picker')

  def testUserWhitelistAndAllowNewUsers(self):
    """Checks that login can be (dis)allowed by whitelist and allow-new-users.

    The test verifies that these two interrelated policies behave as documented
    in the chrome/browser/policy/proto/chrome_device_policy.proto file. Cases
    for which the current behavior is marked as "broken" are intentionally
    ommitted since the broken behavior should be fixed rather than protected by
    tests.
    """
    # No whitelist
    self.SetDevicePolicy({'allow_new_users': True})
    self.Login(user_index=0, expect_success=True)
    self.Logout()

    # Empty whitelist
    self.SetDevicePolicy({'user_whitelist': []})
    self.Login(user_index=0, expect_success=True)
    self.Logout()

    self.SetDevicePolicy({'allow_new_users': True,
                          'user_whitelist': []})
    self.Login(user_index=0, expect_success=True)
    self.Logout()

    # Populated whitelist
    self.SetDevicePolicy({'user_whitelist': [self._usernames[0]]})
    self.Login(user_index=0, expect_success=True)
    self.Logout()
    self.Login(user_index=1, expect_success=False)

    self.SetDevicePolicy({'allow_new_users': True,
                          'user_whitelist': [self._usernames[0]]})
    self.Login(user_index=0, expect_success=True)
    self.Logout()
    self.Login(user_index=1, expect_success=True)
    self.Logout()

    # New users not allowed, populated whitelist
    self.SetDevicePolicy({'allow_new_users': False,
                          'user_whitelist': [self._usernames[0]]})
    self.Login(user_index=0, expect_success=True)
    self.Logout()
    self.Login(user_index=1, expect_success=False)

  def testUserWhitelistInAccountPicker(self):
    """Checks that setting a whitelist removes non-whitelisted user pods."""
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self.TryToDisableLocalStateAutoClearing()
    if self._local_state_auto_clearing:
      logging.warn('Unable to disable local state clearing. Skipping test.')
      return

    # Disable the account picker so that the login form is shown and the Login()
    # automation call can be used.
    self.PrepareToWaitForLoginFormReload()
    self.SetDevicePolicy({'show_user_names': False})
    self.WaitForLoginFormReload()

    # Log in to populate the list of existing users.
    self.Login(user_index=0, expect_success=True)
    self.Logout()
    self.Login(user_index=1, expect_success=True)
    self.Logout()

    # Enable the account picker.
    self.SetDevicePolicy({'show_user_names': True})
    self._WaitForLoginScreenId('account-picker')

    # Check pod visibility with and without a whitelist.
    self._WaitForPodVisibility(username=self._usernames[0], visible=True)
    self._WaitForPodVisibility(username=self._usernames[1], visible=True)

    self.SetDevicePolicy({'show_user_names': True,
                          'user_whitelist': [self._usernames[1]]})
    self._WaitForPodVisibility(username=self._usernames[0], visible=False)
    self._WaitForPodVisibility(username=self._usernames[1], visible=True)

    self.SetDevicePolicy({'show_user_names': True})
    self._WaitForPodVisibility(username=self._usernames[0], visible=True)
    self._WaitForPodVisibility(username=self._usernames[1], visible=True)


if __name__ == '__main__':
  pyauto_functional.Main()
