// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "canvasviewbroker.h"
#include "canvasmanager.h"
#include "view/canvasview.h"
#include "view/canvasview_p.h"
#include "delegate/canvasitemdelegate_p.h"

#include <dfm-framework/dpf.h>

Q_DECLARE_METATYPE(QRect *)
Q_DECLARE_METATYPE(QList<QUrl> *)

using namespace ddplugin_canvas;

#define CanvasViewSlot(topic, args...) \
            dpfSlotChannel->connect(QT_STRINGIFY(DDP_CANVAS_NAMESPACE), QT_STRINGIFY2(topic), this, ##args)

#define CanvasViewDisconnect(topic) \
            dpfSlotChannel->disconnect(QT_STRINGIFY(DDP_CANVAS_NAMESPACE), QT_STRINGIFY2(topic))

CanvasViewBroker::CanvasViewBroker(CanvasManager *mrg, QObject *parent)
    : QObject(parent)
    , manager(mrg)
{

}

CanvasViewBroker::~CanvasViewBroker()
{
    CanvasViewDisconnect(slot_CanvasView_VisualRect);
    CanvasViewDisconnect(slot_CanvasView_GridPos);
    CanvasViewDisconnect(slot_CanvasView_Refresh);
    CanvasViewDisconnect(slot_CanvasView_Update);
    CanvasViewDisconnect(slot_CanvasView_Select);
    CanvasViewDisconnect(slot_CanvasView_SelectedUrls);
    CanvasViewDisconnect(slot_CanvasView_GridSize);
    CanvasViewDisconnect(slot_CanvasView_GridVisualRect);

    CanvasViewDisconnect(slot_CanvasItemDelegate_IconRect);
}

bool CanvasViewBroker::init()
{
    CanvasViewSlot(slot_CanvasView_VisualRect, &CanvasViewBroker::visualRect);
    CanvasViewSlot(slot_CanvasView_GridPos, &CanvasViewBroker::gridPos);
    CanvasViewSlot(slot_CanvasView_Refresh, &CanvasViewBroker::refresh);
    CanvasViewSlot(slot_CanvasView_Update, &CanvasViewBroker::update);
    CanvasViewSlot(slot_CanvasView_Select, &CanvasViewBroker::select);
    CanvasViewSlot(slot_CanvasView_SelectedUrls, &CanvasViewBroker::selectedUrls);
    CanvasViewSlot(slot_CanvasView_GridSize, &CanvasViewBroker::gridSize);
    CanvasViewSlot(slot_CanvasView_GridVisualRect, &CanvasViewBroker::gridVisualRect);

    CanvasViewSlot(slot_CanvasItemDelegate_IconRect, &CanvasViewBroker::iconRect);
    return true;
}

QSharedPointer<CanvasView> CanvasViewBroker::getView(int idx)
{
    // screen num is start with 1
    for (auto v : manager->views())
        if (v->screenNum() == idx)
            return v;

    return nullptr;
}

QRect CanvasViewBroker::visualRect(int idx, const QUrl &url)
{
    QRect rect;
    if (auto view = getView(idx)) {
        QPoint gridPos;
        if (view->d->itemGridpos(url.toString(), gridPos))
            return view->d->visualRect(gridPos);
    }
    return rect;
}

QRect CanvasViewBroker::gridVisualRect(int idx, const QPoint &gridPos)
{
    QRect rect;
    if (auto view = getView(idx))
        rect = view->d->visualRect(gridPos);
    return rect;
}

QPoint CanvasViewBroker::gridPos(int idx, const QPoint &viewPoint)
{
    QPoint pos;
    if (auto view = getView(idx))
        pos = view->d->gridAt(viewPoint);
    return pos;
}

QSize CanvasViewBroker::gridSize(int idx)
{
    QSize size;
    if (auto view = getView(idx))
        size = QSize(view->d->canvasInfo.columnCount, view->d->canvasInfo.rowCount);

    return size;
}

void CanvasViewBroker::refresh(int idx)
{
    if (auto view = getView(idx))
        view->refresh(true);
}

void CanvasViewBroker::update(int idx)
{
    if (idx < 0) {
        for (auto view : manager->views())
            view->update();
        return;
    }

    if (auto view = getView(idx))
        view->update();
}

void CanvasViewBroker::select(const QList<QUrl> &urls)
{
    QItemSelection selection;
    auto m = manager->model();
    for (const QUrl &url: urls) {
        auto index = m->index(url);
        if (index.isValid())
            selection.append(QItemSelectionRange(index));
    }

    manager->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
}

QList<QUrl> CanvasViewBroker::selectedUrls(int idx)
{
    QList<QUrl> urls;
    auto files = manager->selectionModel()->selectedUrls();
    if (idx < 0) {
        urls = files;
    } else if (auto view = getView(idx)) {
        const int num = view->screenNum();
        QStringList items;
        items << GridIns->points(num).keys();
        items << GridIns->overloadItems(num);

        QList<QUrl> viewOn;
        for (const QUrl &url: files) {
            if (items.contains(url.toString()))
                viewOn.append(url);
        }
        urls = viewOn;
    }
    return urls;
}

QRect CanvasViewBroker::iconRect(int idx, QRect visualRect)
{
    QRect ret;
    if (auto view = getView(idx)) {
        visualRect = visualRect.marginsRemoved(view->d->gridMargins);
        ret = view->itemDelegate()->iconRect(visualRect);
    }
    return ret;
}

