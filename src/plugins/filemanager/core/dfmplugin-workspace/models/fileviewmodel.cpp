// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileviewmodel.h"
#include "views/fileview.h"
#include "utils/workspacehelper.h"
#include "utils/fileoperatorhelper.h"
#include "utils/filedatahelper.h"
#include "events/workspaceeventsequence.h"
#include "filesortfilterproxymodel.h"

#include "base/application/settings.h"
#include "dfm-base/dfm_global_defines.h"
#include "dfm-base/utils/fileutils.h"
#include "dfm-base/utils/sysinfoutils.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/utils/fileinfohelper.h"

#include <dfm-framework/event/event.h>

#include <QApplication>
#include <QPointer>
#include <QList>
#include <QMimeData>

Q_DECLARE_METATYPE(QList<QUrl> *)

DFMGLOBAL_USE_NAMESPACE
DFMBASE_USE_NAMESPACE
using namespace dfmplugin_workspace;

inline constexpr int kMaxColumnCount { 10 };

FileViewModel::FileViewModel(QAbstractItemView *parent)
    : QAbstractItemModel(parent),
      fileDataHelper(new FileDataHelper(this))

{
    connect(&FileInfoHelper::instance(), &FileInfoHelper::createThumbnailFinished, this, &FileViewModel::onFileUpdated);
}

FileViewModel::~FileViewModel()
{
}

QModelIndex FileViewModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        if (row < 0 || column < 0)
            return QModelIndex();
        auto info = fileDataHelper->findRootInfo(row);
        if (!info || !info->data)
            return QModelIndex();

        return createIndex(row, column, info->data);
    }

    auto data = fileDataHelper->findFileItemData(parent.row(), row);
    if (data)
        return createIndex(row, column, data);

    return QModelIndex();
}

QUrl FileViewModel::rootUrl(const QModelIndex &rootIndex) const
{
    auto rootInfo = fileDataHelper->findRootInfo(rootIndex.row());
    if (rootInfo)
        return rootInfo->url;

    return QUrl();
}

QModelIndex FileViewModel::rootIndex(const QUrl &rootUrl) const
{
    const RootInfo *info = fileDataHelper->findRootInfo(rootUrl);
    return createIndex(info->rowIndex, 0, info->data);
}

QModelIndex FileViewModel::setRootUrl(const QUrl &url, FileView *view)
{
    if (!url.isValid())
        return QModelIndex();

    int rootRow = fileDataHelper->preSetRoot(url);
    RootInfo *root = nullptr;
    if (rowCount() <= rootRow) {
        // insert root index
        beginInsertRows(QModelIndex(), rootRow, rootRow);
        root = fileDataHelper->setRoot(url);
        endInsertRows();

        connect(root, &RootInfo::insert, this, &FileViewModel::onInsert, Qt::QueuedConnection);
        connect(root, &RootInfo::insertFinish, this, &FileViewModel::onInsertFinish, Qt::QueuedConnection);
        connect(root, &RootInfo::remove, this, &FileViewModel::onRemove, Qt::QueuedConnection);
        connect(root, &RootInfo::removeFinish, this, &FileViewModel::onRemoveFinish, Qt::QueuedConnection);
        connect(root, &RootInfo::childrenUpdate, this, &FileViewModel::childrenUpdated, Qt::QueuedConnection);
        connect(root, &RootInfo::selectAndEditFile, this, &FileViewModel::selectAndEditFile, Qt::QueuedConnection);
        connect(root, &RootInfo::reloadView, this, &FileViewModel::reloadView, Qt::QueuedConnection);
    } else {
        root = fileDataHelper->setRoot(url);
    }

    const QModelIndex &index = rootIndex(url);

    if (WorkspaceHelper::instance()->haveViewRoutePrehandler(url.scheme())) {
        root->canFetchByForce = true;
        Q_EMIT traverPrehandle(url, index, view);
    } else {
        root->canFetchMore = true;
        fetchMore(index);
    }

    return index;
}

QModelIndex FileViewModel::findRootIndex(const QUrl &url) const
{
    RootInfo *info = fileDataHelper->findRootInfo(url);
    if (info)
        return index(info->rowIndex, 0, QModelIndex());

    return QModelIndex();
}

QModelIndex FileViewModel::findChildIndex(const QUrl &rootUrl, const QUrl &url) const
{
    auto indexPair = fileDataHelper->getChildIndexByUrl(rootUrl, url);

    if (indexPair.first < 0 || indexPair.second < 0)
        return QModelIndex();

    const QModelIndex &parentIndex = index(indexPair.first, 0, QModelIndex());

    return index(indexPair.second, 0, parentIndex);
}

void FileViewModel::doFetchMore(const QModelIndex &rootIndex)
{
    auto rootInfo = fileDataHelper->findRootInfo(rootIndex.row());
    if (!rootInfo->canFetchByForce)
        return;

    rootInfo->canFetchByForce = false;
    rootInfo->canFetchMore = true;
    rootInfo->needTraversal = true;

    fetchMore(rootIndex);
}

AbstractFileInfoPointer FileViewModel::fileInfo(const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0)
        return nullptr;

    const QModelIndex &parentIndex = index.parent();
    if (!parentIndex.isValid()) {
        RootInfo *info = fileDataHelper->findRootInfo(index.row());

        if (info && info->data)
            return info->data->fileInfo();

        return nullptr;
    }

    const FileItemData *data = fileDataHelper->findFileItemData(parentIndex.row(), index.row());
    if (data)
        return data->fileInfo();

    return nullptr;
}

AbstractFileInfoPointer FileViewModel::fileInfo(const QModelIndex &parent, const QModelIndex &index) const
{
    if (!index.isValid() || index.row() < 0)
        return nullptr;

    if (!parent.isValid()) {
        RootInfo *info = fileDataHelper->findRootInfo(index.row());

        if (info && info->data)
            return info->data->fileInfo();

        return nullptr;
    }

    const FileItemData *data = fileDataHelper->findFileItemData(parent.row(), index.row());
    if (data)
        return data->fileInfo();

    return nullptr;
}

QList<QUrl> FileViewModel::getChildrenUrls(const QUrl &rootUrl) const
{
    const RootInfo *info = fileDataHelper->findRootInfo(rootUrl);
    if (info)
        return info->getChildrenUrls();

    return {};
}

QModelIndex FileViewModel::parent(const QModelIndex &child) const
{
    const FileItemData *childData = static_cast<FileItemData *>(child.internalPointer());

    if (childData && childData->parentData()
        && childData->parentData()->fileInfo()) {
        return findRootIndex(childData->parentData()->fileInfo()->urlOf(UrlInfoType::kUrl));
    }

    return QModelIndex();
}

int FileViewModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return fileDataHelper->rootsCount();

    return fileDataHelper->filesCount(parent.row());
}

int FileViewModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return kMaxColumnCount;

    const RootInfo *info = fileDataHelper->findRootInfo(parent.row());
    if (info)
        return getColumnRoles(info->url).count();

    return 0;
}

QVariant FileViewModel::data(const QModelIndex &index, int role) const
{
    const QModelIndex &parentIndex = index.parent();
    if (!parentIndex.isValid()) {
        RootInfo *info = fileDataHelper->findRootInfo(index.row());
        if (info)
            return info->data->data(role);
        else
            return QVariant();
    }

    FileItemData *itemData = fileDataHelper->findFileItemData(parentIndex.row(), index.row());

    if (itemData && itemData->fileInfo())
        return itemData->data(role);
    else
        return QVariant();
}

void FileViewModel::clear(const QUrl &rootUrl)
{
    fileDataHelper->clear(rootUrl);
}

void FileViewModel::update(const QUrl &rootUrl)
{
    fileDataHelper->update(rootUrl);

    RootInfo *info = fileDataHelper->findRootInfo(rootUrl);
    if (info) {
        const QModelIndex &parentIndex = findRootIndex(rootUrl);
        emit dataChanged(index(0, 0, parentIndex), index(info->childrenCount(), 0, parentIndex));
    }
}

void FileViewModel::refresh(const QUrl &rootUrl)
{
    auto rootInfo = fileDataHelper->findRootInfo(rootUrl);
    if (rootInfo) {
        fileDataHelper->clear(rootUrl);

        rootInfo->canFetchMore = true;
        const QModelIndex &index = rootIndex(rootUrl);
        fetchMore(index);
    }
}

void FileViewModel::fetchMore(const QModelIndex &parent)
{
    if (!canFetchMore(parent)) {
        QApplication::restoreOverrideCursor();
        return;
    }

    traversRootDir(parent);
}

bool FileViewModel::canFetchMore(const QModelIndex &parent) const
{
    auto rootInfo = fileDataHelper->findRootInfo(parent.row());
    if (rootInfo)
        return rootInfo->canFetchMore;

    return false;
}

Qt::ItemFlags FileViewModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    const AbstractFileInfoPointer &info = fileInfo(index);
    if (!info)
        return flags;

    if (info->canAttributes(CanableInfoType::kCanRename))
        flags |= Qt::ItemIsEditable;

    if (info->isAttributes(OptInfoType::kIsWritable)) {
        if (info->canAttributes(CanableInfoType::kCanDrop))
            flags |= Qt::ItemIsDropEnabled;
        else
            flags |= Qt::ItemNeverHasChildren;
    }

    if (info->canAttributes(CanableInfoType::kCanDrag))
        flags |= Qt::ItemIsDragEnabled;

    return flags;
}

QStringList FileViewModel::mimeTypes() const
{
    return QStringList(QLatin1String("text/uri-list"));
}

QMimeData *FileViewModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls;
    QSet<QUrl> urlsSet;
    QList<QModelIndex>::const_iterator it = indexes.begin();

    for (; it != indexes.end(); ++it) {
        if ((*it).column() == 0) {
            const AbstractFileInfoPointer &fileInfo = this->fileInfo(*it);
            const QUrl &url = fileInfo->urlOf(UrlInfoType::kUrl);

            if (urlsSet.contains(url))
                continue;

            urls << url;
            urlsSet << url;
        }
    }

    QMimeData *data = new QMimeData();

    data->setUrls(urls);

    QByteArray userID;
    userID.append(QString::number(SysInfoUtils::getUserId()));
    data->setData(DFMGLOBAL_NAMESPACE::Mime::kDataUserIDKey, userID);

    return data;
}

bool FileViewModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    const QModelIndex &dropIndex = index(row, column, parent);

    if (!dropIndex.isValid())
        return false;

    const AbstractFileInfoPointer &targetFileInfo = fileInfo(dropIndex);
    if (targetFileInfo->isAttributes(OptInfoType::kIsDir) && !targetFileInfo->isAttributes(OptInfoType::kIsWritable)) {
        qInfo() << "current dir is not writable!!!!!!!!";
        return false;
    }
    QUrl targetUrl = targetFileInfo->urlOf(UrlInfoType::kUrl);
    const QList<QUrl> &dropUrls = data->urls();

    if (targetFileInfo->isAttributes(OptInfoType::kIsSymLink))
        targetUrl = QUrl::fromLocalFile(targetFileInfo->pathOf(PathInfoType::kSymLinkTarget));

    FileView *view = qobject_cast<FileView *>(qobject_cast<QObject *>(this)->parent());

    if (FileUtils::isTrashDesktopFile(targetUrl)) {
        FileOperatorHelperIns->moveToTrash(view, dropUrls);
        return true;
    } else if (FileUtils::isDesktopFile(targetUrl)) {
        FileOperatorHelperIns->openFilesByApp(view, dropUrls, QStringList { targetUrl.toLocalFile() });
        return true;
    }

    bool ret { true };

    switch (action) {
    case Qt::CopyAction:
        [[fallthrough]];
    case Qt::MoveAction:
        if (dropUrls.count() > 0)
            // call move
            FileOperatorHelperIns->dropFiles(view, action, targetUrl, dropUrls);
        break;
    default:
        break;
    }

    return ret;
}

void FileViewModel::traversRootDir(const QModelIndex &rootIndex)
{
    if (rootIndex.isValid())
        fileDataHelper->doTravers(rootIndex.row());
}

void FileViewModel::stopTraversWork(const QUrl &rootUrl)
{
    fileDataHelper->doStopWork(rootUrl);
    Q_EMIT stateChanged(rootUrl, ModelState::kIdle);
}

QList<ItemRoles> FileViewModel::getColumnRoles(const QUrl &rootUrl) const
{
    QList<ItemRoles> roles;
    bool customOnly = WorkspaceEventSequence::instance()->doFetchCustomColumnRoles(rootUrl, &roles);

    const QVariantMap &map = DFMBASE_NAMESPACE::Application::appObtuselySetting()->value("FileViewState", rootUrl).toMap();
    if (map.contains("headerList")) {
        QVariantList headerList = map.value("headerList").toList();

        for (ItemRoles role : roles) {
            if (!headerList.contains(role))
                headerList.append(role);
        }

        roles.clear();
        for (auto var : headerList) {
            roles.append(static_cast<ItemRoles>(var.toInt()));
        }
    } else if (!customOnly) {
        static QList<ItemRoles> defualtColumnRoleList = QList<ItemRoles>() << kItemFileDisplayNameRole
                                                                           << kItemFileLastModifiedRole
                                                                           << kItemFileSizeRole
                                                                           << kItemFileMimeTypeRole;

        int customCount = roles.count();
        for (auto role : defualtColumnRoleList) {
            if (!roles.contains(role))
                roles.insert(roles.length() - customCount, role);
        }
    }

    return roles;
}

void FileViewModel::setIndexActive(const QModelIndex &index, bool enable)
{
    fileDataHelper->setFileActive(index.parent().row(), index.row(), enable);
}

void FileViewModel::cleanDataCacheByUrl(const QUrl &url)
{
    fileDataHelper->clear(url);
}

void FileViewModel::updateRoot(const QList<QUrl> urls)
{
    for (const auto &url : urls) {
        fileDataHelper->updateRootInfoStatus(QString(), url.path());
    }
}

void FileViewModel::updateFile(const QUrl &root, const QUrl &url)
{
    const QModelIndex &index = findChildIndex(root, url);
    if (index.isValid()) {
        auto info = InfoFactory::create<AbstractFileInfo>(url);
        if (info)
            info->refresh();

        emit dataChanged(index, index);
    }
}

void FileViewModel::onFilesUpdated()
{
    FileView *view = qobject_cast<FileView *>(qobject_cast<QObject *>(this)->parent());
    if (view) {
        QDir::Filters filter = view->model()->getFilters();
        view->model()->setFilters(filter);
    }
    emit updateFiles();
}

void FileViewModel::onFileUpdated(const QUrl &url)
{
    const QUrl &rootUrl = UrlRoute::urlParent(url);
    updateFile(rootUrl, url);
}

void FileViewModel::onInsert(int rootIndex, int firstIndex, int count)
{
    const QModelIndex &parentIndex = index(rootIndex, 0, QModelIndex());

    beginInsertRows(parentIndex, firstIndex, firstIndex + count - 1);
}

void FileViewModel::onInsertFinish()
{
    endInsertRows();
}

void FileViewModel::onRemove(int rootIndex, int firstIndex, int count)
{
    const QModelIndex &parentIndex = index(rootIndex, 0, QModelIndex());

    beginRemoveRows(parentIndex, firstIndex, firstIndex + count - 1);
}

void FileViewModel::onRemoveFinish()
{
    endRemoveRows();
}
