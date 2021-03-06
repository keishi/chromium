// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_

#include <string>

#include "base/string16.h"
#include "base/time.h"
#include "ui/gfx/font.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/text_button.h"

namespace views {

class MouseEvent;
class MenuButtonListener;


////////////////////////////////////////////////////////////////////////////////
//
// MenuButton
//
//  A button that shows a menu when the left mouse button is pushed
//
////////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT MenuButton : public TextButton {
 public:
  static const char kViewClassName[];

  // Create a Button.
  MenuButton(ButtonListener* listener,
             const string16& text,
             MenuButtonListener* menu_button_listener,
             bool show_menu_marker);
  virtual ~MenuButton();

  void set_menu_marker(const gfx::ImageSkia* menu_marker) {
    menu_marker_ = menu_marker;
  }

  const gfx::Point& menu_offset() const { return menu_offset_; }
  void set_menu_offset(int x, int y) { menu_offset_.SetPoint(x, y); }

  // Activate the button (called when the button is pressed).
  virtual bool Activate();

  // Overridden from TextButton for the potential use of a drop marker.
  virtual void PaintButton(gfx::Canvas* canvas, PaintButtonMode mode) OVERRIDE;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual ui::GestureStatus OnGestureEvent(const GestureEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // True if the menu is currently visible.
  bool menu_visible_;

  // Offset of the associated menu position.
  gfx::Point menu_offset_;

 private:
  friend class TextButtonBackground;

  // Compute the maximum X coordinate for the current screen. MenuButtons
  // use this to make sure a menu is never shown off screen.
  int GetMaximumScreenXCoordinate();

  // We use a time object in order to keep track of when the menu was closed.
  // The time is used for simulating menu behavior for the menu button; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::Time menu_closed_time_;

  // Our listener. Not owned.
  MenuButtonListener* listener_;

  // Whether or not we're showing a drop marker.
  bool show_menu_marker_;

  // The down arrow used to differentiate the menu button from normal
  // text buttons.
  const gfx::ImageSkia* menu_marker_;

  // If non-null the destuctor sets this to true. This is set while the menu is
  // showing and used to detect if the menu was deleted while running.
  bool* destroyed_flag_;

  DISALLOW_COPY_AND_ASSIGN(MenuButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_MENU_BUTTON_H_
