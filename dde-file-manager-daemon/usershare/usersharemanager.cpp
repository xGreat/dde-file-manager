/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
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

#include "usersharemanager.h"
#include "dbusservice/dbusadaptor/usershare_adaptor.h"
#include <QDBusConnection>
#include <QDBusVariant>
#include <QProcess>
#include <QDebug>
#include "app/policykithelper.h"

QString UserShareManager::ObjectPath = "/com/deepin/filemanager/daemon/UserShareManager";
QString UserShareManager::PolicyKitActionId = "com.deepin.filemanager.daemon.UserShareManager";

UserShareManager::UserShareManager(QObject *parent)
    : QObject(parent)
    , QDBusContext()
{
    QDBusConnection::systemBus().registerObject(ObjectPath, this);
    m_userShareAdaptor = new UserShareAdaptor(this);
}

UserShareManager::~UserShareManager()
{

}

bool UserShareManager::checkAuthentication()
{
    bool ret = false;
    qint64 pid = 0;
    QDBusConnection c = QDBusConnection::connectToBus(QDBusConnection::SystemBus, "org.freedesktop.DBus");
    if(c.isConnected()) {
       pid = c.interface()->servicePid(message().service()).value();
    }

    if (pid){
        ret = PolicyKitHelper::instance()->checkAuthorization(PolicyKitActionId, pid);
    }

    if (!ret) {
        qDebug() << "Authentication failed !!";
    }
    return ret;
}

bool UserShareManager::addGroup(const QString &groupName)
{
    if (!checkAuthentication()) {
        qDebug() << "addGroup failed" <<  groupName;
        return false;
    }

    QStringList args;
    args << groupName;
    bool ret = QProcess::startDetached("/usr/sbin/groupadd", args);
    qDebug() << "groupadd" << groupName << ret;
    return ret;
}

bool UserShareManager::setUserSharePassword(const QString &username, const QString &passward)
{
    if (!checkAuthentication()) {
        qDebug() << "setUserSharePassword failed" <<  username;
        return false;
    }

    qDebug() << username;// << passward; // log password?
    QStringList args;
    args << "-a" << username << "-s";
    QProcess p;
    p.start("smbpasswd", args);
    p.write(passward.toStdString().c_str());
    p.write("\n");
    p.write(passward.toStdString().c_str());
    p.closeWriteChannel();
    bool ret = p.waitForFinished();
    qDebug() << p.readAll() << p.readAllStandardError() << p.readAllStandardOutput();
    return ret;
}

bool UserShareManager::closeSmbShareByShareName(const QString &sharename, const bool bshow)
{
    if (!bshow) {
        return true;
    }
    if (!checkAuthentication()) {
        qDebug() << "closeSmbShareByShareName failed" <<  sharename;
        return false;
    }

    qDebug() << sharename;
    QProcess p;
    //取得所有连击的pid
    QString cmd = QString("smbcontrol smbd close-share %1").arg(sharename);
    qDebug() << "cmd==========" << cmd;
    p.start(cmd);
    bool ret = p.waitForFinished();

    qDebug() << p.readAll() << p.readAllStandardError() << p.readAllStandardOutput();
    return ret;
}
