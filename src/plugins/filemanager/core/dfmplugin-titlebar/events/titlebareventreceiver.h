// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TITLEBAREVENTRECEIVER_H
#define TITLEBAREVENTRECEIVER_H

#include "dfmplugin_titlebar_global.h"

#include <QObject>

namespace dfmplugin_titlebar {

class TitleBarEventReceiver : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TitleBarEventReceiver)

public:
    static TitleBarEventReceiver *instance();

public slots:
    // receive other plugin signals
    void handleTabAdded(quint64 windowId);
    void handleTabChanged(quint64 windowId, int index);
    void handleTabMoved(quint64 windowId, int from, int to);
    void handleTabRemovd(quint64 windowId, int index);

    // self slots
    bool handleCustomRegister(const QString &scheme, const QVariantMap &properties);

    void handleStartSpinner(quint64 windowId);
    void handleStopSpinner(quint64 windowId);

    void handleShowFilterButton(quint64 windowId, bool visible);
    void handleViewModeChanged(quint64 windowId, int mode);

    void handleSetNewWindowAndTabEnable(bool enable);
    void handleWindowForward(quint64 windowId);
    void handleWindowBackward(quint64 windowId);

private:
    explicit TitleBarEventReceiver(QObject *parent = nullptr);
};

}

#endif   // TITLEBAREVENTRECEIVER_H
