/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     liyigang<liyigang@uniontech.com>
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
#include "localfilehandler.h"

#include <QString>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QDateTime>

#include <unistd.h>
#include <utime.h>
#include <cstdio>

DFMBASE_USE_NAMESPACE

LocalFileHandler::LocalFileHandler() {}

LocalFileHandler::~LocalFileHandler()
{
    if (errorStr) {
        delete errorStr;
        errorStr = nullptr;
    }
}
/*!
 * \brief LocalFileHandler::touchFile 创建文件，创建文件时会使用Truncate标志，清理掉文件的内容
 * 如果文件存在就会造成文件内容丢失
 * \param url 新文件的url
 * \return bool 创建文件是否成功
 */
bool LocalFileHandler::touchFile(const QUrl &url)
{
    QFile file(url.toLocalFile());

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.close();
    } else {
        QString strErr = QObject::tr("Unable to create files here: %1").arg(file.errorString());
        setError(strErr);
        return false;
    }
    return true;
}
/*!
 * \brief LocalFileHandler::mkdir 创建目录
 * \param dir 创建目录的url
 * \return bool 创建文件是否成功
 */
bool LocalFileHandler::mkdir(const QUrl &dir)
{
    if (!QDir::current().mkpath(dir.toLocalFile())) {
        QString strErr = QObject::tr("Unable to create files here: %1").arg(strerror(errno));
        return false;
    }

    return true;
}
/*!
 * \brief LocalFileHandler::rmdir 删除目录
 * \param url 删除目录的url
 * \return bool 是否删除目录成功
 */
bool LocalFileHandler::rmdir(const QUrl &url)
{
    if (::rmdir(url.toLocalFile().toLocal8Bit()) == 0)
        return true;

    setError(QString::fromLocal8Bit(strerror(errno)));

    return false;
}
/*!
 * \brief LocalFileHandler::rename 重命名文件
 * \param url 源文件的url
 * \param newUrl 新文件的url
 * \return bool 重命名文件是否成功
 */
bool LocalFileHandler::renameFile(const QUrl &url, const QUrl &newUrl)
{
    const QByteArray &sourceFile = url.toLocalFile().toLocal8Bit();
    const QByteArray &targetFile = newUrl.toLocalFile().toLocal8Bit();

    if (::rename(sourceFile.constData(), targetFile.constData()) == 0)
        return true;

    setError(QString::fromLocal8Bit(strerror(errno)));

    return false;
}
/*!
 * \brief LocalFileHandler::openFile 打开文件
 * \param file 打开文件的url
 * \return bool 打开文件是否成功
 */
bool LocalFileHandler::openFile(const QUrl &file)
{
    // Todo:: open file
    Q_UNUSED(file);
    return true;
}
/*!
 * \brief LocalFileHandler::openFiles 打开多个文件
 * \param files 打开文件的url列表
 * \return bool 打开文件是否成功
 */
bool LocalFileHandler::openFiles(const QList<QUrl> &files)
{
    // Todo:: open files
    Q_UNUSED(files);
    return true;
}
/*!
 * \brief LocalFileHandler::openFileByApp 指定的app打开文件
 * \param file 文件的url
 * \param appDesktop app的desktop路径
 * \return bool 是否打开成功
 */
bool LocalFileHandler::openFileByApp(const QUrl &file, const QString &appDesktop)
{
    // Todo:: open file by app
    Q_UNUSED(file);
    Q_UNUSED(appDesktop);
    return true;
}
/*!
 * \brief LocalFileHandler::openFilesByApp 指定的app打开多个文件
 * \param files 文件的url列表
 * \param appDesktop app的desktop路径
 * \return bool 是否打开成功
 */
bool LocalFileHandler::openFilesByApp(const QList<QUrl> &files, const QString &appDesktop)
{
    // Todo:: open files by app
    Q_UNUSED(files);
    Q_UNUSED(appDesktop);
    return true;
}

/*!
 * \brief LocalFileHandler::createSystemLink 创建文件的连接文件使用系统c库
 * \param sourcfile 源文件的url
 * \param link 链接文件的url
 * \return bool 创建链接文件是否成功
 */
bool LocalFileHandler::createSystemLink(const QUrl &sourcfile, const QUrl &link)
{
    if (::symlink(sourcfile.path().toLocal8Bit().constData(),
                  link.toLocalFile().toLocal8Bit().constData())
        == 0)
        return true;

    setError(QString::fromLocal8Bit(strerror(errno)));

    return false;
}
/*!
 * \brief LocalFileHandler::setPermissions 设置文件的权限
 * \param url 需要设置文件的url
 * \param permissions 文件的权限
 * \return bool 设置文件的权限是否成功
 */
bool LocalFileHandler::setPermissions(const QUrl &url, QFileDevice::Permissions permissions)
{
    QFile file(url.toLocalFile());

    if (file.setPermissions(permissions))
        return true;

    setError(file.errorString());

    return false;
}
/*!
 * \brief LocalFileHandler::deleteFile 删除文件使用系统c库
 * \param file 文件的url
 * \return bool 删除文件是否失败
 */
bool LocalFileHandler::deleteFile(const QUrl &file)
{
    if (::remove(file.toLocalFile().toLocal8Bit()) == 0)
        return true;

    setError(QString::fromLocal8Bit(strerror(errno)));

    return false;
}
/*!
 * \brief LocalFileHandler::setFileTime 设置文件的读取和最后修改时间
 * \param url 文件的url
 * \param accessDateTime 文件的读取时间
 * \param lastModifiedTime 文件最后修改时间
 * \return bool 设置是否成功
 */
bool LocalFileHandler::setFileTime(const QUrl &url, const QDateTime &accessDateTime,
                                   const QDateTime &lastModifiedTime)
{
    utimbuf buf = { accessDateTime.toTime_t(), lastModifiedTime.toTime_t() };

    if (::utime(url.toLocalFile().toLocal8Bit(), &buf) == 0) {
        return true;
    }

    setError(QString::fromLocal8Bit(strerror(errno)));

    return false;
}
/*!
 * \brief LocalFileHandler::errorString 获取错误信息
 * \return
 */
QString LocalFileHandler::errorString()
{
    if (errorStr)
        return *errorStr;

    return QString();
}
/*!
 * \brief LocalFileHandler::setError 设置当前的错误信息
 * \param error 错误信息
 */
void LocalFileHandler::setError(const QString &error)
{
    if (errorStr)
        errorStr = new QString;

    *errorStr = error;
}
