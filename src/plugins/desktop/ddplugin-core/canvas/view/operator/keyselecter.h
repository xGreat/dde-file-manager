/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangyu<zhangyub@uniontech.com>
 *
 * Maintainer: zhangyu<zhangyub@uniontech.com>
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
#ifndef KEYSELECTER_H
#define KEYSELECTER_H
#include "dfm_desktop_service_global.h"
#include "clickselecter.h"

#include <QObject>
#include <QModelIndex>

class QKeyEvent;
DSB_D_BEGIN_NAMESPACE
class CanvasView;
class KeySelecter : private ClickSelecter
{
    Q_OBJECT
public:
    explicit KeySelecter(CanvasView *parent);
    void keyPressed(QKeyEvent *event);
    static const QList<Qt::Key> &filterKeys();
protected:
    QPersistentModelIndex moveCursor(QKeyEvent *event) const;
    void singleSelect(const QModelIndex &index);
    void incrementSelect(const QModelIndex &index);
signals:

public slots:
protected:
    CanvasView *view;
};

DSB_D_END_NAMESPACE
#endif // KEYSELECTER_H