// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WINDOWUTILS_H
#define WINDOWUTILS_H

#include "dfm-base/dfm_base_global.h"

namespace dfmbase {

class WindowUtils
{
public:
    static bool isWayLand();

    static bool keyShiftIsPressed();

    static bool keyCtrlIsPressed();

    static bool keyAltIsPressed();
};

}
#endif   // WINDOWUTILS_H
