// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

class AppNotificationBridge;

// A rounded window with an arrow used for example when you click on the STAR
// button or that pops up within our first-run UI.
@interface InfoBubbleWindow : ChromeEventProcessingWindow {
 @private
  // Is self in the process of closing.
  BOOL closing_;
  // If NO the window will close immediately instead of fading out.
  // Default YES.
  BOOL delayOnClose_;
  // If NO the window will never become key.
  // Default YES.
  BOOL canBecomeKeyWindow_;
  // If YES then things under the window will look blurred.
  // Default YES.
  BOOL hasBlurredBackground_;
  // Bridge to proxy Chrome notifications to the window.
  scoped_ptr<AppNotificationBridge> notificationBridge_;
}

@property(nonatomic) BOOL delayOnClose;
@property(nonatomic) BOOL canBecomeKeyWindow;
// Only has an effect if called before the bubble is shown.
@property(nonatomic) BOOL hasBlurredBackground;

// Returns YES if the window is in the process of closing.
// Can't use "windowWillClose" notification because that will be sent
// after the closing animation has completed.
- (BOOL)isClosing;

@end
