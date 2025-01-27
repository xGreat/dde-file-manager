// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filesortfilterproxymodel.h"
#include "fileviewmodel.h"
#include "utils/workspacehelper.h"
#include "views/fileview.h"

#include "base/application/settings.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/utils/fileutils.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/widgets/dfmwindow/filemanagerwindowsmanager.h"
#include "dfm-base/base/device/deviceproxymanager.h"

#include <QTimer>

DFMBASE_USE_NAMESPACE
DFMGLOBAL_USE_NAMESPACE
using namespace dfmplugin_workspace;

FileSortFilterProxyModel::FileSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    resetFilter();

    initMixDirAndFile();
}

FileSortFilterProxyModel::~FileSortFilterProxyModel()
{
}

QVariant FileSortFilterProxyModel::headerData(int column, Qt::Orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        int column_role = getRoleByColumn(column);
        return roleDisplayString(column_role);
    }

    return QVariant();
}

Qt::ItemFlags FileSortFilterProxyModel::flags(const QModelIndex &index) const
{
    // single select mode will check index enabled when remove row
    if (!index.isValid())
        return Qt::ItemFlag::ItemIsEnabled;

    Qt::ItemFlags flags = QSortFilterProxyModel::flags(index);

    const AbstractFileInfoPointer &info = itemFileInfo(index);

    if (info) {
        if (!passNameFilters(info))
            flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }

    if (readOnly)
        flags &= ~(Qt::ItemIsEditable | Qt::ItemIsDropEnabled | Qt::ItemNeverHasChildren);

    return flags;
}

Qt::DropActions FileSortFilterProxyModel::supportedDragActions() const
{
    const QModelIndex &rootIndex = viewModel()->findRootIndex(rootUrl);
    const AbstractFileInfoPointer info = viewModel()->fileInfo(rootIndex);

    if (info)
        return info->supportedOfAttributes(SupportedType::kDrag);

    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

Qt::DropActions FileSortFilterProxyModel::supportedDropActions() const
{
    const QModelIndex &rootIndex = viewModel()->findRootIndex(rootUrl);
    const AbstractFileInfoPointer info = viewModel()->fileInfo(rootIndex);

    if (info)
        return info->supportedOfAttributes(SupportedType::kDrop);

    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

int FileSortFilterProxyModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        const QModelIndex &index = viewModel()->findRootIndex(rootUrl);
        return sourceModel()->columnCount(index);
    }

    return QSortFilterProxyModel::columnCount(parent);
}

QModelIndex FileSortFilterProxyModel::setRootUrl(const QUrl &url)
{
    rootUrl = url;

    workStoped = false;

    auto view = qobject_cast<FileView *>(parent());
    currentSourceIndex = viewModel()->setRootUrl(url, view);
    resetFilter();

    return mapFromSource(currentSourceIndex);
}

QUrl FileSortFilterProxyModel::currentRootUrl() const
{
    return rootUrl;
}

void FileSortFilterProxyModel::clear()
{
    viewModel()->clear(rootUrl);
}

void FileSortFilterProxyModel::update()
{
    viewModel()->update(rootUrl);
}

void FileSortFilterProxyModel::refresh()
{
    if (state != ModelState::kIdle)
        return;

    initMixDirAndFile();
    viewModel()->refresh(rootUrl);
}

void FileSortFilterProxyModel::updateFile(const QUrl &url)
{
    viewModel()->updateFile(rootUrl, url);
}

AbstractFileInfoPointer FileSortFilterProxyModel::itemFileInfo(const QModelIndex &index) const
{
    const QModelIndex &sourceIndex = mapToSource(index);
    return viewModel()->fileInfo(sourceIndex);
}

QModelIndex FileSortFilterProxyModel::getIndexByUrl(const QUrl &url) const
{
    const QModelIndex &sourceIndex = viewModel()->findChildIndex(rootUrl, url);
    const QModelIndex &proxyIndex = mapFromSource(sourceIndex);

    if (proxyIndex.isValid())
        return proxyIndex;

    return QModelIndex();
}

QUrl FileSortFilterProxyModel::getUrlByIndex(const QModelIndex &index) const
{
    const QModelIndex &sourceIndex = mapToSource(index);
    return sourceIndex.data(kItemUrlRole).toUrl();
}

QList<QUrl> FileSortFilterProxyModel::getCurrentDirFileUrls() const
{
    return viewModel()->getChildrenUrls(rootUrl);
}

int FileSortFilterProxyModel::getColumnWidth(const int &column) const
{
    const ItemRoles role = getRoleByColumn(column);

    const QVariantMap &state = Application::appObtuselySetting()->value("WindowManager", "ViewColumnState").toMap();
    int colWidth = state.value(QString::number(role), -1).toInt();
    if (colWidth > 0) {
        return colWidth;
    }

    return 120;
}

ItemRoles FileSortFilterProxyModel::getRoleByColumn(const int &column) const
{
    QList<ItemRoles> columnRoleList = viewModel()->getColumnRoles(rootUrl);

    if (columnRoleList.length() > column)
        return columnRoleList.at(column);

    return kItemFileDisplayNameRole;
}

int FileSortFilterProxyModel::getColumnByRole(const ItemRoles role) const
{
    QList<ItemRoles> columnRoleList = viewModel()->getColumnRoles(rootUrl);
    return columnRoleList.indexOf(role) < 0 ? 0 : columnRoleList.indexOf(role);
}

QDir::Filters FileSortFilterProxyModel::getFilters() const
{
    return filters;
}

QStringList FileSortFilterProxyModel::getNameFilters() const
{
    return nameFilters;
}

void FileSortFilterProxyModel::setFilters(const QDir::Filters &filters)
{
    this->filters = filters;

    invalidateFilter();
}

void FileSortFilterProxyModel::setNameFilters(const QStringList &nameFilters)
{
    if (this->nameFilters == nameFilters) {
        return;
    }

    nameFiltersMatchResultMap.clear();
    this->nameFilters = nameFilters;

    invalidateFilter();
}

void FileSortFilterProxyModel::setFilterData(const QVariant &data)
{
    filterData = data;
    invalidateFilter();
}

void FileSortFilterProxyModel::setFilterCallBack(const FileViewFilterCallback callback)
{
    filterCallback = callback;
    invalidateFilter();
}

void FileSortFilterProxyModel::resetFilter()
{
    filterData = QVariant();
    filterCallback = nullptr;
    filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    bool isShowedHiddenFiles = Application::instance()->genericAttribute(Application::kShowedHiddenFiles).toBool();
    if (isShowedHiddenFiles) {
        filters |= QDir::Hidden;
    } else {
        filters &= ~QDir::Hidden;
    }

    if (currentSourceIndex.isValid())
        invalidateFilter();
}

void FileSortFilterProxyModel::toggleHiddenFiles()
{
    filters = ~(filters ^ QDir::Filter(~QDir::Hidden));
    setFilters(filters);
}

void FileSortFilterProxyModel::setReadOnly(const bool readOnly)
{
    this->readOnly = readOnly;
}

void FileSortFilterProxyModel::stopWork()
{
    workStoped = true;
    viewModel()->stopTraversWork(rootUrl);
}

void FileSortFilterProxyModel::setActive(const QModelIndex &index, bool enable)
{
    const QModelIndex &sourceIndex = mapToSource(index);
    viewModel()->setIndexActive(sourceIndex, enable);
}

void FileSortFilterProxyModel::updateRootInfo(const QList<QUrl> &urls)
{
    viewModel()->updateRoot(urls);
}

ModelState FileSortFilterProxyModel::currentState() const
{
    return state;
}

void FileSortFilterProxyModel::initMixDirAndFile()
{
    isNotMixDirAndFile = !Application::instance()->appAttribute(Application::kFileAndDirMixedSort).toBool();
}

void FileSortFilterProxyModel::onChildrenUpdate(const QUrl &url)
{
    if (UniversalUtils::urlEquals(url, rootUrl))
        Q_EMIT modelChildrenUpdated();
}

void FileSortFilterProxyModel::onTraverPrehandle(const QUrl &url, const QModelIndex &index, const FileView *view)
{
    if (UniversalUtils::urlEquals(url, rootUrl) && view == parent()) {
        auto prehandler = WorkspaceHelper::instance()->viewRoutePrehandler(url.scheme());
        if (prehandler) {
            isPrehandling = true;
            quint64 winId = FileManagerWindowsManager::instance().findWindowId(dynamic_cast<FileView *>(parent()));
            QPointer<FileViewModel> guard(viewModel());
            prehandler(winId, url, [guard, index, this]() {
                if (guard)
                    guard->doFetchMore(index);

                this->isPrehandling = false;
            });
        }
    }
}

void FileSortFilterProxyModel::onStateChanged(const QUrl &url, ModelState state)
{
    if (UniversalUtils::urlEquals(url, rootUrl)) {
        if (state == this->state || isPrehandling)
            return;

        this->state = state;

        if (workStoped)
            return;

        Q_EMIT stateChanged();
    }
}

void FileSortFilterProxyModel::onSelectAndEditFile(const QUrl &rootUrl, const QUrl &url)
{
    if (UniversalUtils::urlEquals(rootUrl, this->rootUrl)) {
        quint64 winId = WorkspaceHelper::instance()->windowId(dynamic_cast<QWidget *>(parent()));
        if (WorkspaceHelper::kSelectionAndRenameFile.contains(winId)) {
            if (WorkspaceHelper::kSelectionAndRenameFile[winId].first == rootUrl) {
                WorkspaceHelper::kSelectionAndRenameFile[winId] = qMakePair(QUrl(), QUrl());

                QTimer::singleShot(100, this, [=] {
                    emit selectAndEditFile(url);
                });
            }
        }
    }
}

bool FileSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (!left.isValid())
        return false;
    if (!right.isValid())
        return false;

    const QModelIndex &leftParent = left.parent();
    if (!leftParent.isValid() || !UniversalUtils::urlEquals(viewModel()->fileInfo(QModelIndex(), leftParent)->urlOf(UrlInfoType::kUrl), rootUrl))
        return false;
    const QModelIndex &rightParent = right.parent();
    if (!rightParent.isValid() || !UniversalUtils::urlEquals(viewModel()->fileInfo(QModelIndex(), rightParent)->urlOf(UrlInfoType::kUrl), rootUrl))
        return false;

    const AbstractFileInfoPointer &leftInfo = viewModel()->fileInfo(leftParent, left);
    const AbstractFileInfoPointer &rightInfo = viewModel()->fileInfo(rightParent, right);

    if (!leftInfo)
        return false;
    if (!rightInfo)
        return false;

    // The folder is fixed in the front position
    if (isNotMixDirAndFile) {
        if (leftInfo->isAttributes(OptInfoType::kIsDir)) {
            if (!rightInfo->isAttributes(OptInfoType::kIsDir))
                return sortOrder() == Qt::AscendingOrder;
        } else {
            if (rightInfo->isAttributes(OptInfoType::kIsDir))
                return sortOrder() == Qt::DescendingOrder;
        }
    }

    QVariant leftData = viewModel()->data(left, sortRole());
    QVariant rightData = viewModel()->data(right, sortRole());

    // When the selected sort attribute value is the same, sort by file name
    if (leftData == rightData) {
        QString leftName = viewModel()->data(left, kItemFileDisplayNameRole).toString();
        QString rightName = viewModel()->data(right, kItemFileDisplayNameRole).toString();
        return FileUtils::compareString(leftName, rightName, sortOrder());
    }

    switch (sortRole()) {
    case kItemFileDisplayNameRole:
    case kItemFileLastModifiedRole:
    case kItemFileMimeTypeRole:
        return FileUtils::compareString(leftData.toString(), rightData.toString(), sortOrder()) == (sortOrder() == Qt::AscendingOrder);
    case kItemFileSizeRole:
        if (isNotMixDirAndFile) {
            if (leftInfo->isAttributes(OptInfoType::kIsDir)) {
                int leftCount = qSharedPointerDynamicCast<AbstractFileInfo>(leftInfo)->countChildFile();
                int rightCount = qSharedPointerDynamicCast<AbstractFileInfo>(rightInfo)->countChildFile();
                return leftCount < rightCount;
            } else {
                return leftInfo->size() < rightInfo->size();
            }
        } else {
            qint64 sizel = leftInfo->isAttributes(OptInfoType::kIsDir) && rightInfo->isAttributes(OptInfoType::kIsDir)
                    ? qSharedPointerDynamicCast<AbstractFileInfo>(leftInfo)->countChildFile()
                    : (leftInfo->isAttributes(OptInfoType::kIsDir) ? 0 : leftInfo->size());
            qint64 sizer = leftInfo->isAttributes(OptInfoType::kIsDir) && rightInfo->isAttributes(OptInfoType::kIsDir)
                    ? qSharedPointerDynamicCast<AbstractFileInfo>(rightInfo)->countChildFile()
                    : (rightInfo->isAttributes(OptInfoType::kIsDir) ? 0 : rightInfo->size());
            return sizel < sizer;
        }
    default:
        return FileUtils::compareString(leftData.toString(), rightData.toString(), sortOrder()) == (sortOrder() == Qt::AscendingOrder);
    }
}

bool FileSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex rowIndex = viewModel()->index(sourceRow, 0, sourceParent);

    if (!rowIndex.isValid())
        return false;

    // root index should not be filtered
    if (!sourceParent.isValid())
        return true;

    // only filter current index rows
    if (sourceParent != currentSourceIndex)
        return false;

    const AbstractFileInfoPointer &fileInfo = viewModel()->fileInfo(rowIndex);

    return passFileFilters(fileInfo);
}

bool FileSortFilterProxyModel::passFileFilters(const AbstractFileInfoPointer &fileInfo) const
{
    if (!fileInfo)
        return false;

    if (filterCallback && !filterCallback(fileInfo.data(), filterData))
        return false;

    if (filters == QDir::NoFilter)
        return true;

    if (!(filters & (QDir::Dirs | QDir::AllDirs)) && fileInfo->isAttributes(OptInfoType::kIsDir))
        return false;

    if (!(filters & QDir::Files) && fileInfo->isAttributes(OptInfoType::kIsFile))
        return false;

    if ((filters & QDir::NoSymLinks) && fileInfo->isAttributes(OptInfoType::kIsSymLink))
        return false;

    if (!(filters & QDir::Hidden)
        && (fileInfo->isAttributes(OptInfoType::kIsHidden) || isDefaultHiddenFile(fileInfo))) {
        return false;
    }

    if ((filters & QDir::Readable) && !fileInfo->isAttributes(OptInfoType::kIsReadable))
        return false;

    if ((filters & QDir::Writable) && !fileInfo->isAttributes(OptInfoType::kIsWritable))
        return false;

    if ((filters & QDir::Executable) && !fileInfo->isAttributes(OptInfoType::kIsExecutable))
        return false;

    return true;
}

bool FileSortFilterProxyModel::passNameFilters(const AbstractFileInfoPointer &info) const
{
    if (nameFilters.isEmpty())
        return true;

    if (!info)
        return true;

    const QUrl fileUrl = info->urlOf(UrlInfoType::kUrl);
    if (nameFiltersMatchResultMap.contains(fileUrl))
        return nameFiltersMatchResultMap.value(fileUrl, false);

    // Check the name regularexpression filters
    if (!(info->isAttributes(OptInfoType::kIsDir) && (filters & QDir::Dirs))) {
        const Qt::CaseSensitivity caseSensitive = (filters & QDir::CaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive;
        const QString &fileName = info->nameOf(NameInfoType::kFileName);
        QRegExp re("", caseSensitive, QRegExp::Wildcard);

        for (int i = 0; i < nameFilters.size(); ++i) {
            re.setPattern(nameFilters.at(i));
            if (re.exactMatch(fileName)) {
                nameFiltersMatchResultMap[fileUrl] = true;
                return true;
            }
        }

        nameFiltersMatchResultMap[fileUrl] = false;
        return false;
    }

    nameFiltersMatchResultMap[fileUrl] = true;
    return true;
}

bool FileSortFilterProxyModel::isDefaultHiddenFile(const AbstractFileInfoPointer &info) const
{
    static QSet<QUrl> defaultHiddenUrls;
    static std::once_flag flg;
    std::call_once(flg, [&] {
        using namespace GlobalServerDefines;
        auto systemBlks = DevProxyMng->getAllBlockIds(DeviceQueryOption::kSystem | DeviceQueryOption::kMounted);
        for (const auto &blk : systemBlks) {
            auto blkInfo = DevProxyMng->queryBlockInfo(blk);
            QStringList mountPoints = blkInfo.value(DeviceProperty::kMountPoints).toStringList();
            for (const auto &mpt : mountPoints) {
                defaultHiddenUrls.insert(QUrl::fromLocalFile(mpt + (mpt == "/" ? "root" : "/root")));
                defaultHiddenUrls.insert(QUrl::fromLocalFile(mpt + (mpt == "/" ? "lost+found" : "/lost+found")));
            }
        }
    });
    return defaultHiddenUrls.contains(info->urlOf(UrlInfoType::kUrl));
}

FileViewModel *FileSortFilterProxyModel::viewModel() const
{
    return qobject_cast<FileViewModel *>(sourceModel());
}

QString FileSortFilterProxyModel::roleDisplayString(int role) const
{
    QString displayName;

    if (WorkspaceEventSequence::instance()->doFetchCustomRoleDiaplayName(rootUrl, static_cast<ItemRoles>(role), &displayName))
        return displayName;

    switch (role) {
    case kItemFileDisplayNameRole:
        return tr("Name");
    case kItemFileLastModifiedRole:
        return tr("Time modified");
    case kItemFileSizeRole:
        return tr("Size");
    case kItemFileMimeTypeRole:
        return tr("Type");
    default:
        return QString();
    }
}

QList<ItemRoles> FileSortFilterProxyModel::getColumnRoles() const
{
    return viewModel()->getColumnRoles(rootUrl);
}
