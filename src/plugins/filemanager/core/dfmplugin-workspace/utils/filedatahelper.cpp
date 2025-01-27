// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filedatahelper.h"
#include "models/fileviewmodel.h"

#include "dfm-base/dfm_global_defines.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/base/schemefactory.h"
#include "dfm-base/base/urlroute.h"
#include "dfm-base/base/device/deviceproxymanager.h"

#include <QApplication>

using namespace dfmbase;
using namespace dfmbase::Global;
using namespace dfmplugin_workspace;

RootInfo *FileDataHelper::setRoot(const QUrl &rootUrl)
{
    if (rootInfoMap.contains(rootUrl)) {
        if (rootUrl.scheme() == Scheme::kFile && !rootInfoMap[rootUrl]->needTraversal)
            return rootInfoMap[rootUrl];

        RootInfo *info = rootInfoMap[rootUrl];
        info->clearChildren();
        info->needTraversal = true;

        const AbstractFileWatcherPointer &watcher = setWatcher(rootUrl);
        info->watcher = watcher;
        info->startWatcher();

        return info;
    }

    RootInfo *info = createRootInfo(rootUrl);
    rootInfoMap[rootUrl] = info;

    info->startWatcher();

    return rootInfoMap[rootUrl];
}

RootInfo *FileDataHelper::findRootInfo(const QUrl &url)
{
    if (rootInfoMap.contains(url))
        return rootInfoMap[url];

    return nullptr;
}

RootInfo *FileDataHelper::findRootInfo(const int rowIndex)
{
    auto iter = rootInfoMap.begin();
    for (; iter != rootInfoMap.end(); ++iter) {
        if (rowIndex == iter.value()->rowIndex)
            return iter.value();
    }

    return nullptr;
}

FileItemData *FileDataHelper::findFileItemData(const int rootIndex, const int childIndex)
{
    auto root = findRootInfo(rootIndex);
    if (root && root->childrenCount() > childIndex)
        return root->fileCache->getChild(childIndex);

    return nullptr;
}

QPair<int, int> FileDataHelper::getChildIndexByUrl(const QUrl &rootUrl, const QUrl &url)
{
    if (!rootUrl.isValid() || !url.isValid())
        return QPair<int, int>(-1, -1);

    if (rootInfoMap.contains(rootUrl))
        return QPair<int, int>(rootInfoMap[rootUrl]->rowIndex, rootInfoMap[rootUrl]->childIndex(url));

    return QPair<int, int>(-1, -1);
}

int FileDataHelper::rootsCount()
{
    return rootInfoMap.count();
}

int FileDataHelper::filesCount(const int rootIndex)
{
    auto info = findRootInfo(rootIndex);
    if (info)
        return info->childrenCount();

    return 0;
}

void FileDataHelper::doTravers(const int rootIndex)
{
    auto info = findRootInfo(rootIndex);

    if (info && info->needTraversal) {
        model()->stateChanged(info->url, ModelState::kBusy);

        info->canFetchMore = false;
        info->needTraversal = false;

        setTraversalThread(info);

        if (!info->fileCache.isNull()) {
            info->fileCache->stop();
            info->fileCache->disconnect();
            info->fileCache->deleteLater();
        }
        info->fileCache.reset(new FileDataCacheThread(info));

        connect(info->traversal.data(), &TraversalDirThread::updateChild,
                info->fileCache.data(), &FileDataCacheThread::onHandleAddFile,
                Qt::QueuedConnection);
        connect(info->traversal.data(), &TraversalDirThread::updateChildren,
                this, [this, info](QList<AbstractFileInfoPointer> children) {
                    if (children.isEmpty())
                        this->model()->stateChanged(info->url, ModelState::kIdle);
                });
        connect(info->traversal.data(), &QThread::finished,
                info->fileCache.data(), &FileDataCacheThread::onHandleTraversalFinished,
                Qt::QueuedConnection);
        connect(
                info->fileCache.data(), &FileDataCacheThread::requestSetIdle,
                this, [this, info] { this->model()->stateChanged(info->url, ModelState::kIdle); },
                Qt::QueuedConnection);

        info->traversal->start();
    } else {
        info->canFetchMore = false;
        QApplication::restoreOverrideCursor();
    }
}

void FileDataHelper::doStopWork(const QUrl &rootUrl)
{
    RootInfo *info = findRootInfo(rootUrl);
    if (info) {

        if (!info->traversal.isNull() && info->traversal->isRunning()) {
            info->traversal->stop();
            info->needTraversal = true;
        }

        if (!info->fileCache.isNull() && info->fileCache->isRunning())
            info->fileCache->stop();
    }
}

void FileDataHelper::update(const QUrl &rootUrl)
{
    if (rootInfoMap.contains(rootUrl))
        rootInfoMap[rootUrl]->refreshChildren();
}

void FileDataHelper::clear(const QUrl &rootUrl)
{
    RootInfo *info = findRootInfo(rootUrl);
    if (info) {
        info->needTraversal = true;
        info->clearChildren();
    }
}

void FileDataHelper::setFileActive(const int rootIndex, const int childIndex, bool active)
{
    RootInfo *info = findRootInfo(rootIndex);
    if (info) {
        FileItemData *data = info->fileCache->getChild(childIndex);
        if (data && data->fileInfo() && info->watcher)
            info->watcher->setEnabledSubfileWatcher(data->fileInfo()->urlOf(UrlInfoType::kUrl), active);
    }
}

void FileDataHelper::updateRootInfoStatus(const QString &, const QString &mountPoint)
{
    auto mpt = QUrl::fromLocalFile(mountPoint);
    const QList<QUrl> &&caches = rootInfoMap.keys();
    for (const auto &url : caches) {
        // if parent node is updated, then all the children should be updated too.
        if (mpt.isParentOf(url) || UniversalUtils::urlEquals(mpt, url)) {
            auto &node = rootInfoMap[url];
            if (node) {
                node->needTraversal = true;
                qDebug() << "rootinfo is updated on device mounted: " << mpt << url;
            }
        }
    }
}

FileViewModel *FileDataHelper::model() const
{
    return dynamic_cast<FileViewModel *>(parent());
}

RootInfo *FileDataHelper::createRootInfo(const QUrl &url)
{
    const AbstractFileWatcherPointer &watcher = setWatcher(url);

    int index = rootInfoMap.count();

    RootInfo *info = new RootInfo(index, url, watcher);

    return info;
}

FileDataHelper::FileDataHelper(QObject *parent)
    : QObject(parent)
{
    // unmount a protocol device will not trigger 'fileDeleted' signal, so re-enter
    // a protocol device may not travers the inner files.
    // FIXME(xust) FIXME(liuyangming) caches should be removed when device unmounted.
    connect(DevProxyMng, &DeviceProxyManager::blockDevMounted, this, &FileDataHelper::updateRootInfoStatus);
    connect(DevProxyMng, &DeviceProxyManager::protocolDevMounted, this, &FileDataHelper::updateRootInfoStatus);
}

int FileDataHelper::preSetRoot(const QUrl &rootUrl)
{
    if (rootInfoMap.contains(rootUrl))
        return rootInfoMap[rootUrl]->rowIndex;

    return rootInfoMap.count();
}

AbstractFileWatcherPointer FileDataHelper::setWatcher(const QUrl &url)
{
    if (rootInfoMap.count(url) && !rootInfoMap[url]->watcher.isNull()) {
        const AbstractFileWatcherPointer &watcher = rootInfoMap[url]->watcher;

        watcher->disconnect();
        watcher->stopWatcher();
    }

    const AbstractFileWatcherPointer &watcher = WatcherFactory::create<AbstractFileWatcher>(url);
    if (watcher.isNull()) {
        qWarning() << "Create watcher failed! url = " << url;
    }

    return watcher;
}

TraversalThreadPointer FileDataHelper::setTraversalThread(RootInfo *info)
{
    destroyTraversalThread(info->traversal);

    TraversalThreadPointer traversal(new TraversalDirThread(
            info->url, QStringList(),
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
            QDirIterator::FollowSymlinks));

    info->traversal = traversal;
    return traversal;
}

void FileDataHelper::destroyTraversalThread(TraversalThreadPointer threadPtr)
{
    if (threadPtr.isNull())
        return;

    threadPtr->disconnect();
    threadPtr->setParent(nullptr);

    if (threadPtr->isFinished()) {
        threadPtr->deleteLater();
        return;
    }

    // delete the thread ptr through remove last ref of it
    // while it finished after called stop()
    waitToDestroyThread.append(threadPtr);
    connect(threadPtr.data(), &QThread::finished, this,
            [=]() {
                threadPtr->disconnect();
                waitToDestroyThread.removeAll(threadPtr);
            },
            Qt::QueuedConnection);

    threadPtr->quit();
}
