﻿/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef FINALLYUTIL_H
#define FINALLYUTIL_H

#include "dfm-base/dfm_base_global.h"

#include <QObject>

#include <functional>

DFMBASE_BEGIN_NAMESPACE

class FinallyUtil
{
    Q_DISABLE_COPY(FinallyUtil)

public:
    explicit FinallyUtil(std::function<void()> onExit);
    ~FinallyUtil();
    void dismiss(bool dismissed = true);

private:
    std::function<void()> exitFunc;
    bool hasDismissed { false };
};

DFMBASE_END_NAMESPACE

#endif   // FINALLYUTIL_H