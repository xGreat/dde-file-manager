// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SMBSHAREITERATOR_P_H
#define SMBSHAREITERATOR_P_H

#include "dfmplugin_smbbrowser_global.h"

#include <dfm-io/dfmio_global.h>

BEGIN_IO_NAMESPACE
class DLocalEnumerator;
END_IO_NAMESPACE

namespace dfmplugin_smbbrowser {

class SmbShareIterator;
class SmbShareIteratorPrivate
{
    friend class SmbShareIterator;

public:
    explicit SmbShareIteratorPrivate(const QUrl &url, SmbShareIterator *qq);
    ~SmbShareIteratorPrivate();

private:
    SmbShareIterator *q { nullptr };
    SmbShareNodes smbShares;
    QScopedPointer<DFMIO::DLocalEnumerator> enumerator { nullptr };
};

}

#endif   // SMBSHAREITERATOR_P_H
