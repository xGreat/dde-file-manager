// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNITLIST_H
#define UNITLIST_H

#include "core/upgradeunit.h"

// units
#include "headerunit.h"
#include "dconfigupgradeunit.h"
#include "bookmarkupgradeunit.h"
#include "tagdbupgradeunit.h"

// units end

#include <QList>
#include <QSharedPointer>

#define RegUnit(unit) \
    QSharedPointer<UpgradeUnit>(new unit)

namespace dfm_upgrade {

inline QList<QSharedPointer<UpgradeUnit>> createUnits()
{
    return QList<QSharedPointer<UpgradeUnit>> {
        RegUnit(dfm_upgrade::HeaderUnit),
        RegUnit(dfm_upgrade::DConfigUpgradeUnit),
        RegUnit(dfm_upgrade::BookMarkUpgradeUnit),
        RegUnit(dfm_upgrade::TagDbUpgradeUnit)
    };
}

}

#endif   // UNITLIST_H