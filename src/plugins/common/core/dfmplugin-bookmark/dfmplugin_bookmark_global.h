// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DFMPLUGIN_BOOKMARK_GLOBAL_H
#define DFMPLUGIN_BOOKMARK_GLOBAL_H

#define DPBOOKMARK_NAMESPACE dfmplugin_bookmark

#define DPBOOKMARK_BEGIN_NAMESPACE namespace DPBOOKMARK_NAMESPACE {
#define DPBOOKMARK_END_NAMESPACE }
#define DPBOOKMARK_USE_NAMESPACE using namespace DPBOOKMARK_NAMESPACE;

DPBOOKMARK_BEGIN_NAMESPACE

namespace BookmarkActionId {
inline constexpr char kActAddBookmarkKey[] { "add-bookmark" };
inline constexpr char kActRemoveBookmarkKey[] { "remove-bookmark" };
}

DPBOOKMARK_END_NAMESPACE

#endif   // DFMPLUGIN_BOOKMARK_GLOBAL_H
