// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace internal {

// DisplayChangeListener implementation for aura. Cancels the menu any time the
// root window bounds change.
class AuraDisplayChangeListener
    : public DisplayChangeListener,
      public aura::WindowObserver {
 public:
  AuraDisplayChangeListener(Widget* widget, MenuRunner* menu_runner);
  virtual ~AuraDisplayChangeListener();

  // aura::WindowObserver overrides:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

 private:
  MenuRunner* menu_runner_;
  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(AuraDisplayChangeListener);
};

AuraDisplayChangeListener::AuraDisplayChangeListener(Widget* widget,
                                                     MenuRunner* menu_runner)
    : menu_runner_(menu_runner),
      root_window_(widget->GetNativeView()->GetRootWindow()) {
  if (root_window_)
    root_window_->AddObserver(this);
}

AuraDisplayChangeListener::~AuraDisplayChangeListener() {
  if (root_window_)
    root_window_->RemoveObserver(this);
}

void AuraDisplayChangeListener::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  menu_runner_->Cancel();
}

// static
DisplayChangeListener* DisplayChangeListener::Create(Widget* widget,
                                                     MenuRunner* runner) {
  return new AuraDisplayChangeListener(widget, runner);
}

}  // namespace internal
}  // namespace views
