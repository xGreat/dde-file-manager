/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liqiang<liqianga@uniontech.com>
 *
 * Maintainer: liqiang<liqianga@uniontech.com>
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
#include "defaultcanvasmodel.h"
#include "defaultdesktopfileinfo.h"
#include "defaultfiletreater.h"
#include "dfm-base/base/abstractfileinfo.h"

DSB_D_BEGIN_NAMESPACE

DefaultCanvasModel::DefaultCanvasModel(QObject *parent)
    : dfmbase::AbstractCanvasModel(parent)
{
}

QModelIndex DefaultCanvasModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    if (row < 0 || column < 0 || 0 == DefaultFileTreaterCt->fileCount()) {
        return QModelIndex();
    }
    auto fileInfo = DefaultFileTreaterCt->getFileByIndex(row);
    if (!fileInfo) {
        return QModelIndex();
    }
    // 临时处理
    return createIndex(row, column, fileInfo.data());
}

QModelIndex DefaultCanvasModel::index(const QString &fileUrl, int column)
{
    if (!fileUrl.isEmpty())
        return QModelIndex();
    auto fileInfo = DefaultFileTreaterCt->getFileByUrl(fileUrl);
    if (!fileInfo)
        return QModelIndex();

    return createIndexByFileInfo(fileInfo, column);
}

QModelIndex DefaultCanvasModel::index(const DFMDesktopFileInfoPointer &fileInfo, int column) const
{
    if (!fileInfo)
        return QModelIndex();
    return createIndexByFileInfo(fileInfo, column);
}

QModelIndex DefaultCanvasModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    // 用不着此接口
    return QModelIndex();
}

int DefaultCanvasModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return 0;

    return DefaultFileTreaterCt->fileCount();
}

int DefaultCanvasModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 0;
}

QVariant DefaultCanvasModel::data(const QModelIndex &index, int role) const
{
    // todo: 待优化剔除非桌面角色
    if (!index.isValid() || index.model() != this) {
        return QVariant();
    }

    auto indexFileInfo = static_cast<DefaultDesktopFileInfo *>(index.internalPointer());
    if (!indexFileInfo) {
        return QVariant();
    }
    // todo：可能仍有桌面有用不到的role,待排除
    switch (role) {
    case Qt::EditRole:
    case Qt::DisplayRole: {
        const QVariant &d = data(index.sibling(index.row(), 0), Roles::FileDisplayNameRole);

        if (d.canConvert<QString>()) {
            return d;
        } else if (d.canConvert<QPair<QString, QString>>()) {
            return qvariant_cast<QPair<QString, QString>>(d).first;
        } else if (d.canConvert<QPair<QString, QPair<QString, QString>>>()) {
            return qvariant_cast<QPair<QString, QPair<QString, QString>>>(d).first;
        }

        return d;
    }
    case FilePathRole:
    case FileDisplayNameRole:
    case FileNameRole:
    case FileNameOfRenameRole:
    case FileBaseNameRole:
    case FileBaseNameOfRenameRole:
    case FileSuffixRole:
    case FileSuffixOfRenameRole:
    case Qt::TextAlignmentRole:
    case FileLastModifiedRole:
    case FileLastModifiedDateTimeRole:
    case FileSizeRole:
    case FileSizeInKiloByteRole:
    case FileMimeTypeRole:
    case FileCreatedRole:
    case FilePinyinName:
    case ExtraProperties:
        return dataByRole(indexFileInfo, role);
    case FileIconRole:
        return indexFileInfo->fileIcon();
    case Qt::ToolTipRole: {
        // todo:后续桌面tips需求可走这
        return QString();
    }

    default: {
        return QString();
    }
    }
}

QVariant DefaultCanvasModel::dataByRole(const DefaultDesktopFileInfo *fileInfo, int role) const
{
    // todo temp使用
    switch (role) {
    case Roles::FilePathRole:
        return fileInfo->absoluteFilePath();
    case Roles::FileDisplayNameRole:
        return fileInfo->fileDisplayName();
    case Roles::FileNameRole:
        return fileInfo->fileName();
    case Roles::FileBaseNameRole:
        return fileInfo->baseName();
    case Roles::FileSuffixRole:
        return fileInfo->suffix();
    case Qt::TextAlignmentRole:
        return Qt::AlignVCenter;
    case Roles::FileSizeInKiloByteRole:
        return fileInfo->size();
    case Roles::ExtraProperties:
        return fileInfo->extraProperties();
    default: {
        return QVariant();
    }
    }
}

QModelIndex DefaultCanvasModel::createIndexByFileInfo(const DFMDesktopFileInfoPointer &fileInfo, int column) const
{
    int row = (0 < DefaultFileTreaterCt->fileCount()) ? DefaultFileTreaterCt->indexOfChild(fileInfo) : 0;
    return createIndex(row, column, const_cast<DefaultDesktopFileInfo *>(fileInfo.data()));
}

DSB_D_END_NAMESPACE