/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
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
#include "deviceservicehelper.h"

#include "dfm-base/utils/universalutils.h"

#include <dfm-mount/dfmblockmonitor.h>
#include <QDebug>
#include <QGSettings>
#include <QStorageInfo>

Q_GLOBAL_STATIC_WITH_ARGS(dfmbase::Settings, gsGlobal, ("deepin/dde-file-manager", dfmbase::Settings::GenericConfig))

DSC_BEGIN_NAMESPACE

dfmbase::Settings *DeviceServiceHelper::getGsGlobal()
{
    return gsGlobal;
}

std::once_flag &DeviceServiceHelper::autoMountOnceFlag()
{
    static std::once_flag flag;
    return flag;
}

void DeviceServiceHelper::mountAllBlockDevices()
{
    auto ptrList = DeviceServiceHelper::createAllBlockDevices();
    for (auto &blkDev : ptrList)
        mountBlockDeviceAsync(blkDev, {{"auth.no_user_interaction", true}});
}

void DeviceServiceHelper::mountBlockDeviceAsync(BlockDevPtr &blkDev, const QVariantMap &opts)
{
    if (!isMountableBlockDevice(blkDev))
        return;

    blkDev->mountAsync(opts);
}

bool DeviceServiceHelper::mountBlockDevice(DeviceServiceHelper::BlockDevPtr &blkDev, const QVariantMap &opts)
{
    if (!isMountableBlockDevice(blkDev))
        return false;

    return !blkDev->mount(opts).isEmpty();
}

void DeviceServiceHelper::unmountBlockDeviceAsync(DeviceServiceHelper::BlockDevPtr &blkDev, const QVariantMap &opts)
{
    if (!isUnmountableBlockDevice(blkDev))
        return;

    blkDev->unmountAsync(opts);
}

bool DeviceServiceHelper::unmountBlockDevice(DeviceServiceHelper::BlockDevPtr &blkDev, const QVariantMap &opts)
{
    if (!isUnmountableBlockDevice(blkDev))
        return false;

    return blkDev->unmount(opts);
}

void DeviceServiceHelper::mountAllProtocolDevices()
{
    // TODO(zhangs): auto mount protocol devices (wait dfm-mount)
}

bool DeviceServiceHelper::ejectBlockDevice(BlockDevPtr &blkDev)
{
    unmountBlockDevice(blkDev, {});
    if (isEjectableBlockDevice(blkDev))
        return blkDev->eject();

    return false;
}

/*!
 * \brief Not only to eject, but also to poweroff
 */
void DeviceServiceHelper::ejectAllMountedBlockDevices()
{
    auto blkDevcies = DeviceServiceHelper::createAllBlockDevices();
    for (auto &blk : blkDevcies) {
       if (!blk->mountPoints().isEmpty()) {
           if (ejectBlockDevice(blk))
               poweroffBlockDeivceAsync(blk);
       }
    }
}

void DeviceServiceHelper::ejectAllMountedProtocolDevices()
{
    // TODO(zhangs): eject protocal devices (wait dfm-mount)
}

QList<QUrl> DeviceServiceHelper::getMountPathForDrive(const QString &driveName)
{
    QList<QUrl> urls;

    auto blkDevcies = DeviceServiceHelper::createAllBlockDevices();

    for (const auto &device : blkDevcies) {
        if (device->drive() == driveName) {
            const QUrl &url = getMountPathForBlock(device);
            if (url.isValid())
                urls << url;
        }
    }

    return urls;
}

QList<QUrl> DeviceServiceHelper::getMountPathForAllDrive()
{
    QList<QUrl> urls;
    auto blkDevcies = DeviceServiceHelper::createAllBlockDevices();

    for (const auto &device : blkDevcies) {
        const QUrl &url = getMountPathForBlock(device);
        urls << url;
    }

    return urls;
}

QUrl DeviceServiceHelper::getMountPathForBlock(const BlockDevPtr &blkDev)
{
    if (!blkDev)
        return QUrl();
    if (blkDev->mountPoint().isEmpty())
        return QUrl();

    return blkDev->mountPoint();
}

bool DeviceServiceHelper::isUnmountableBlockDevice(const DeviceServiceHelper::BlockDevPtr &blkDev)
{
    if (!blkDev) {
        qWarning() << "Block Device is Null";
        return false;
    }

    if (isProtectedBlocDevice(blkDev)) {
        qWarning() << "Block Device: " << blkDev->path() << " is protected device!";
        return false;
    }

    if (blkDev->fileSystem().isEmpty()) {
        qWarning() << "Block Device: " << blkDev->path() << " haven't a filesystem!";
        return false;
    }

    if (blkDev->mountPoints().isEmpty()) {
        qWarning() << "Block Device: " << blkDev->path() << " not mounted!";
        return false;
    }

    if (blkDev->hintIgnore()) {
        qWarning() << "Block Device: " << blkDev->path() << " hintIgnore!";
        return false;
    }

    bool hintSystem = blkDev->getProperty(DFMMOUNT::Property::BlockHintSystem).toBool();
    if (hintSystem) {
        qWarning() << "Block Device: " << blkDev->path() << " hintSystem!";
        return false;
    }

    return true;
}

bool DeviceServiceHelper::isMountableBlockDevice(const BlockDevPtr &blkDev)
{
    if (!blkDev) {
        qWarning() << "Block Device is Null";
        return false;
    }

    if (blkDev->isEncrypted()) {
        qWarning() << "Block Device: " << blkDev->path() << " is encrypted device!";
        return false;
    }

    if (isProtectedBlocDevice(blkDev)) {
        qWarning() << "Block Device: " << blkDev->path() << " is protected device!";
        return false;
    }

    QString &&cryptoDev = blkDev->getProperty(DFMMOUNT::Property::BlockCryptoBackingDevice).toString();
    if (cryptoDev.length() > 1) { // bug: 77010
        qWarning() << "Block Device: " << blkDev->path() << " cryptoDev length > 1";
        return false;
    }

    if (blkDev->hintIgnore()) {
        qWarning() << "Block Device: " << blkDev->path() << "hintIgnore";
        return false;
    }

    if (!blkDev->mountPoints().isEmpty()) {
        qWarning() << "Block Device: " << blkDev->path() << " has mounted: ";
        return false;
    }

    if (blkDev->fileSystem().isEmpty()) {
        qWarning() << "Block Device: " << blkDev->path() << " haven't a filesystem";
        return false;
    }

    return true;
}

bool DeviceServiceHelper::isEjectableBlockDevice(const DeviceServiceHelper::BlockDevPtr &blkDev)
{
    bool removable = blkDev->removable();
    bool optical = blkDev->optical();
    bool ejectable = blkDev->ejectable();

    qInfo() << "eject" << blkDev->path() << "(removable optical ejectable canPowerOff): "
            << blkDev->mountPoint() << removable << optical << ejectable;

    if (removable)
        return true;

    if (optical && ejectable)
        return true;

    return false;
}

bool DeviceServiceHelper::isProtectedBlocDevice(const BlockDevPtr &blkDev)
{
    QGSettings gsettings("com.deepin.dde.dock.module.disk-mount", "/com/deepin/dde/dock/module/disk-mount/");

    if (gsettings.get("protect-non-media-mounts").toBool()) {
        QStringList &&mountPoints = blkDev->getProperty(DFMMOUNT::Property::FileSystemMountPoint).toStringList();
        for (auto &mountPoint : mountPoints) {
            if (!mountPoint.startsWith("/media/")) {
                return true;
            }
        }
    }

    if (gsettings.get("protect-root-device-mounts").toBool()) {
        QStorageInfo qsi("/");
        auto manager = DFMMOUNT::DFMDeviceManager::instance();
        auto monitor = qobject_cast<QSharedPointer<DFMMOUNT::DFMBlockMonitor>>
                       (manager->getRegisteredMonitor(DFMMOUNT::DeviceType::BlockDevice));
        const QStringList &rootDevNodes = monitor->resolveDeviceNode(qsi.device(), {});
        if (!rootDevNodes.isEmpty()) {
            if (blkDev->drive() == createBlockDevice(rootDevNodes.first())->drive())
                return true;
        }
    }

    return false;
}

DeviceServiceHelper::BlockDevPtr DeviceServiceHelper::createBlockDevice(const QString &devId)
{
     auto devPtr = createDevice(devId, DFMMOUNT::DeviceType::BlockDevice);
     return qobject_cast<BlockDevPtr>(devPtr);
}

DeviceServiceHelper::ProtocolDevPtr DeviceServiceHelper::createProtocolDevice(const QString &devId)
{
    auto devPtr = createDevice(devId, DFMMOUNT::DeviceType::ProtocolDevice);
    return qobject_cast<ProtocolDevPtr>(devPtr);
}

void DeviceServiceHelper::poweroffBlockDeivceAsync(BlockDevPtr &blkDev)
{
    bool canPowerOff = blkDev->canPowerOff();
    if (canPowerOff)
        blkDev->powerOffAsync();
}

DeviceServiceHelper::DevPtr DeviceServiceHelper::createDevice(const QString &devId, DFMMOUNT::DeviceType type)
{
    auto manager = DFMMOUNT::DFMDeviceManager::instance();
    auto monitor = manager->getRegisteredMonitor(type);
    Q_ASSERT_X(monitor, "DeviceServiceHelper", "dfm-mount return a NULL monitor!");
    return monitor->createDeviceById(devId);
}

DeviceServiceHelper::DevPtrList DeviceServiceHelper::createAllDevices(dfmmount::DeviceType type)
{
    DevPtrList list;
    auto manager = DFMMOUNT::DFMDeviceManager::instance();
    const auto &devicesMap = manager->devices(type);
    const QStringList &idList = devicesMap[type];
    for (const QString &id : idList) {
        auto dev = createDevice(id, type);
        if (dev)
            list.append(dev);
    }
    return list;
}

DeviceServiceHelper::BlockDevPtrList DeviceServiceHelper::createAllBlockDevices()
{
    BlockDevPtrList list;
    auto devList = DeviceServiceHelper::createAllDevices(DFMMOUNT::DeviceType::BlockDevice);
    for (auto dev : devList) {
        auto blkDev = qobject_cast<QSharedPointer<DFMMOUNT::DFMBlockDevice>>(dev);
        if (blkDev)
            list.append(blkDev);
    }
    return list;
}

DeviceServiceHelper::ProtocolDevPtrList DeviceServiceHelper::createAllProtocolDevices()
{
    ProtocolDevPtrList list;
    auto devList = DeviceServiceHelper::createAllDevices(DFMMOUNT::DeviceType::ProtocolDevice);
    for (auto dev : devList) {
        auto protocolDev = qobject_cast<QSharedPointer<DFMMOUNT::DFMProtocolDevice>>(dev);
        if (protocolDev)
            list.append(protocolDev);
    }
    return list;
}

DSC_END_NAMESPACE