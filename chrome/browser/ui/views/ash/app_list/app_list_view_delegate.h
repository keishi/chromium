// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_view_delegate.h"

class AppsModelBuilder;
class SearchBuilder;

class AppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  AppListViewDelegate();
  virtual ~AppListViewDelegate();

 private:
  // Overridden from app_list::AppListViewDelegate:
  virtual void SetModel(app_list::AppListModel* model) OVERRIDE;
  virtual void ActivateAppListItem(app_list::AppListItemModel* item,
                                   int event_flags) OVERRIDE;
  virtual void StartSearch() OVERRIDE;
  virtual void StopSearch() OVERRIDE;
  virtual void OpenSearchResult(const app_list::SearchResult& result,
                                int event_flags) OVERRIDE;
  virtual void Close() OVERRIDE;

  scoped_ptr<AppsModelBuilder> apps_builder_;
  scoped_ptr<SearchBuilder> search_builder_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
