// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FRAMEMANAGER_P_H
#define FRAMEANAGER_P_H

#include "ddplugin_organizer_global.h"
#include "framemanager.h"
#include "surface.h"
#include "mode/canvasorganizer.h"
#include "models/collectionmodel.h"
#include "interface/canvasinterface.h"
#include "options/optionswindow.h"
#include "utils/organizerutils.h"

#include <dfm-framework/dpf.h>

namespace ddplugin_organizer {

class FrameManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit FrameManagerPrivate(FrameManager *qq);
    ~FrameManagerPrivate() override;
    void buildSurface();
    void clearSurface();
    SurfacePointer createSurface(QWidget *root);
    void layoutSurface(QWidget *root, SurfacePointer surface, bool hidden = false);
    void buildOrganizer();
    QList<SurfacePointer> surfaces() const;
public slots:
    void refeshCanvas();
public slots:
    void enableChanged(bool e);
    void switchToCustom();
    void switchToNormalized(int cf);
    void displaySizeChanged(int size);
    void showOptionWindow();
public slots:
    bool filterShortcutkeyPress(int viewIndex, int key, int modifiers) const;
    bool filterWheel(int viewIndex, const QPoint &angleDelta, bool ctrl) const;
protected:
    QWidget *findView(QWidget *root) const;
public:
    QMap<QString, SurfacePointer> surfaceWidgets;
    CanvasOrganizer *organizer = nullptr;
    CollectionModel *model = nullptr;
    CanvasInterface *canvas = nullptr;
    OptionsWindow *options = nullptr;
private:
    FrameManager *q;
};

}

#endif // FRAMEMANAGER_P_H
