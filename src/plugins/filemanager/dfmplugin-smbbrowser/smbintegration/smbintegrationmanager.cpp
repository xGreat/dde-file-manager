// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "smbintegrationmanager.h"

#include "dfm-base/dfm_event_defines.h"
#include "events/smbbrowsereventcaller.h"
#include "dfm-base/utils/dialogmanager.h"
#include "dfm-base/utils/sysinfoutils.h"
#include "dfm-base/utils/universalutils.h"

#include "dfm-base/dfm_global_defines.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/base/application/settings.h"
#include "dfm-base/interfaces/abstractfileinfo.h"

#include "dfm-base/base/device/deviceutils.h"
#include "dfm-base/base/device/deviceproxymanager.h"
#include "dfm-base/base/device/devicemanager.h"
#include "dfm-base/file/entry/entryfileinfo.h"
#include "dfm-base/widgets/dfmwindow/filemanagerwindowsmanager.h"

#include <dfm-framework/dpf.h>
#include <dfm-framework/event/event.h>

DFMBASE_USE_NAMESPACE
using namespace dfmplugin_smbbrowser;

using ItemClickedActionCallback = std::function<void(quint64 windowId, const QUrl &url)>;
using FindMeCallback = std::function<bool(const QUrl &itemUrl, const QUrl &targetUrl)>;

Q_DECLARE_METATYPE(ItemClickedActionCallback);
Q_DECLARE_METATYPE(FindMeCallback);

static constexpr char kSmbIntegrationPath[] { "/.smbinteg" };
static constexpr char kSmbIntegrationSuffix[] { "smbinteg" };
static constexpr char kProtocol[] { "protodev" };
static constexpr char kProtodevstashed[] { "protodevstashed" };
static constexpr char kStashedSmbDevices[] { "StashedSmbDevices" };
static constexpr char kSmbIntegrations[] { "SmbIntegrations" };
static constexpr char kSavedPasswordType[] { "SavedPasswordType" };
static constexpr char kGenericAttribute[] { "GenericAttribute" };
static constexpr char kRemoteMounts[] { "RemoteMounts" };
static constexpr char kMergeTheEntriesOfSambaSharedFolders[] { "MergeTheEntriesOfSambaSharedFolders" };
static constexpr char kAlwaysShowOfflineRemoteConnections[] { "AlwaysShowOfflineRemoteConnections" };

static constexpr char kHostKey[] { "host" };
static constexpr char kNameKey[] { "name" };
static constexpr char kProtocolKey[] { "protocol" };
static constexpr char kShareKey[] { "share" };

static constexpr char kGroupNetwork[] { "Group_Network" };

SmbIntegrationManager *SmbIntegrationManager::instance()
{
    static SmbIntegrationManager ins;
    return &ins;
}

bool SmbIntegrationManager::isSmbIntegrationEnabled()
{
    return isSmbIntegrated;
}

bool SmbIntegrationManager::isStashMountsEnabled()
{
    return Application::genericAttribute(Application::kAlwaysShowOfflineRemoteConnections).toBool();
}

void SmbIntegrationManager::removeStashedIntegrationFromConfig(const QUrl &url)
{
    QStringList smbRootUrls = Application::genericSetting()->value(kStashedSmbDevices, kSmbIntegrations).toStringList();
    smbRootUrls.removeOne(url.toString());
    Application::genericSetting()->setValue(kStashedSmbDevices, kSmbIntegrations, smbRootUrls);
}

void SmbIntegrationManager::removeStashedSeperatedItem(const QUrl &url)
{
    const QString &id = url.toString();
    QUrl stashedUrl;
    stashedUrl.setScheme(Global::Scheme::kEntry);
    auto path = id.toUtf8().toBase64();
    QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtodevstashed);
    stashedUrl.setPath(encodecPath);

    dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", stashedUrl);

    // remove from config
    QSet<QString> keys = Application::genericSetting()->keys(kRemoteMounts);
    QString temId = id;
    if (id.endsWith("/"))
        temId.chop(1);
    for (const QString &key : keys) {
        if (key.contains(QUrl(id).host()) && key.endsWith(QUrl(temId).fileName())) {   // TODO:(zhuangshu)
            Application::genericSetting()->remove(kRemoteMounts, key);
            break;
        }
    }
}

void SmbIntegrationManager::handleProtocolDeviceUnmounted(quint64 windowId, const QVariantHash &umountedSmbData)
{
    const QStringList &allProtocolIds = DevProxyMng->getAllProtocolIds();
    const QString &hostOfUmounted = umountedSmbData.value("host").toString();
    QStringList smbHosts;
    for (const auto &dev : allProtocolIds) {
        if (dev.startsWith(Global::Scheme::kSmb) || DeviceUtils::isSamba(dev)) {
            const QUrl &url = QUrl::fromPercentEncoding(dev.toUtf8());
            const QString &path = url.path();
            int pos = path.lastIndexOf("/");
            const QString &displayName = path.mid(pos + 1);
            const QString &host = displayName.section("on", 1, 1).trimmed();
            smbHosts << host;
        }
    }

    // If all mounts are removed under hostOfUmounted, and smb stashed mounts enabled,
    // remove smb integration items from sidebar and computer view
    if (!smbHosts.contains(hostOfUmounted) && isSmbIntegrated && !isSmbStashMountsEnabled) {
        // Remove from sidebar
        QUrl url;
        url.setScheme(Global::Scheme::kEntry);
        url.setHost(hostOfUmounted);
        url.setPath(kSmbIntegrationPath);
        dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
        // Remove from computer view
        dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", url);

        QUrl computerUrl;
        computerUrl.setScheme(Global::Scheme::kComputer);
        SmbBrowserEventCaller::sendChangeCurrentUrl(windowId, computerUrl);
    }
}

SmbIntegrationManager::SmbIntegrationManager(QObject *parent)
    : QObject(parent)
{
    connect(Application::instance(), &Application::genericAttributeChanged, this, &SmbIntegrationManager::onGenAttributeChanged);
    connect(DevProxyMng, &DeviceProxyManager::protocolDevUnmounted, this, &SmbIntegrationManager::onProtocolDeviceUnmounted);
    QSet<QString> keys = Application::genericSetting()->keys(kGenericAttribute);   //
    if (!keys.contains(kMergeTheEntriesOfSambaSharedFolders)) {   // it is the temporary solution, would be handled by settings dialog
        Application::genericSetting()->setValue(kGenericAttribute, kMergeTheEntriesOfSambaSharedFolders, true);
        isSmbIntegrated = true;
    } else {
        isSmbIntegrated = Application::genericSetting()->value(kGenericAttribute, kMergeTheEntriesOfSambaSharedFolders).toBool();
    }

    isSmbStashMountsEnabled = Application::genericSetting()->value(kGenericAttribute, kAlwaysShowOfflineRemoteConnections).toBool();
}

void SmbIntegrationManager::switchIntegrationMode(bool value)
{
    QVariantMap stashedSeperatedData;
    auto getRemoteMounts = [&stashedSeperatedData]() {
        QList<QUrl> list;
        QSet<QString> keys = Application::genericSetting()->keys(kRemoteMounts);
        for (const QString &key : keys) {
            QVariantMap stashedSmbData = Application::genericSetting()->value(kRemoteMounts, key).toMap();
            const QString &protocol = stashedSmbData.value(kProtocolKey).toString();
            const QString &host = stashedSmbData.value(kHostKey).toString();
            const QString &shareName = stashedSmbData.value(kShareKey).toString();
            QString displayName = stashedSmbData.value(kNameKey).toString();
            if (displayName.contains(" on ")) {
                int pos = displayName.lastIndexOf(" on ");
                const QString &shareName = displayName.left(pos);
                const QString &hostName = displayName.right(displayName.length() - pos - 4);
                if (!shareName.isEmpty() && !hostName.isEmpty())
                    displayName = QString("%1 %2 %3").arg(shareName).arg(QObject::tr("on")).arg(hostName);
            }
            QUrl url;
            url.setScheme(protocol);
            url.setHost(host);
            url.setPath("/" + shareName + "/");
            list.append(url);
            if (url.isEmpty() || displayName.isEmpty())
                continue;
            stashedSeperatedData.insert(url.toString(), displayName);
        }
        return list;
    };
    // get stashed smb mount url from `RemoteMounts` field and save to `stashedUrls`.
    QList<QUrl> stashedUrls = getRemoteMounts();   //  list format: smb://1.2.3.4/sharedir/

    if (value)   // switch to smb integration mode
        doSwitchToSmbIntegratedMode(stashedUrls);
    else   // switch to separated smb mode
        doSwitchToSmbSeperatedMode(stashedSeperatedData, stashedUrls);
}

/**
 * @brief SmbIntegrationManager::addSmbIntegrationItem
 * @param hostUrl smb://1.2.3.4 or smb://domain
 */
void SmbIntegrationManager::addSmbIntegrationItem(const QUrl &hostUrl, ContextMenuCallback contexMenu)
{
    if (!hostUrl.isValid() || hostUrl.host().isEmpty() || hostUrl.scheme() != Global::Scheme::kSmb)
        return;

    addIntegrationItemToSidebar(hostUrl, contexMenu);

    addIntegrationItemToComputer(hostUrl);

    QStringList smbRootUrls = Application::genericSetting()->value(kStashedSmbDevices, kSmbIntegrations).toStringList();
    smbRootUrls.append(hostUrl.toString());
    smbRootUrls.removeDuplicates();
    Application::genericSetting()->setValue(kStashedSmbDevices, kSmbIntegrations, smbRootUrls);
}

void SmbIntegrationManager::onOpenItem(quint64 winId, const QUrl &url)
{
    DFMEntryFileInfoPointer info(new EntryFileInfo(url));
    QString suffix = info->nameOf(NameInfoType::kSuffix);
    if (suffix == kSmbIntegrationSuffix) {
        QUrl smbHostUrl;
        smbHostUrl.setScheme(Global::Scheme::kSmb);
        smbHostUrl.setHost(url.host());
        SmbBrowserEventCaller::sendChangeCurrentUrl(winId, smbHostUrl);
    } else if (suffix == kProtodevstashed) {
        suffix = QString(".%1").arg(suffix);
        QString encodecId = url.path().remove(suffix);
        QString id = QByteArray::fromBase64(encodecId.toUtf8());
        dpfSignalDispatcher->publish(GlobalEventType::kChangeCurrentUrl, winId, QUrl(id));
        return;
    }
}

void SmbIntegrationManager::addSmbIntegrationFromConfig(ContextMenuCallback ctxMenuHandle, bool forSidebar)
{
    if (!isSmbIntegrated)
        return;

    QStringList smbRootUrls = Application::genericSetting()->value(kStashedSmbDevices, kSmbIntegrations).toStringList();
    for (const QString &smbRootUrl : smbRootUrls) {
        if (forSidebar)
            addIntegrationItemToSidebar(smbRootUrl, ctxMenuHandle);
        else
            addIntegrationItemToComputer(smbRootUrl);
    }
}

void SmbIntegrationManager::removeSmbIntegrationFromConfig(const QUrl &entryUrl)
{
    QUrl smbRootUrl;
    smbRootUrl.setScheme(Global::Scheme::kSmb);
    smbRootUrl.setHost(entryUrl.host());
    removeStashedIntegrationFromConfig(smbRootUrl);
}

void SmbIntegrationManager::setWindowId(quint64 winId)
{
    windowId = winId;
}

void SmbIntegrationManager::computerOpenItem(quint64 winId, const QUrl &url)
{
    this->onOpenItem(winId, url);
}

void SmbIntegrationManager::onGenAttributeChanged(Application::GenericAttribute ga, const QVariant &value)
{
    if (ga == Application::GenericAttribute::kMergeTheEntriesOfSambaSharedFolders)
        switchIntegrationMode(value.toBool());
    else if (ga == Application::GenericAttribute::kAlwaysShowOfflineRemoteConnections)
        isSmbStashMountsEnabled = value.toBool();
}

void SmbIntegrationManager::onProtocolDeviceUnmounted(const QString &id)
{
    const QUrl &url = QUrl::fromPercentEncoding(id.toUtf8());
    const QString &path = url.path();
    int pos = path.lastIndexOf("/");
    const QString &displayName = path.mid(pos + 1);
    const QString &host = displayName.section("on", 1, 1).trimmed();
    const QString &shareName = displayName.section("on", 0, 0).trimmed();
    QVariantHash umountSmbData;
    umountSmbData.insert(kHostKey, host);
    umountSmbData.insert(kShareKey, shareName);
    umountSmbData.insert(kProtocolKey, Global::Scheme::kSmb);
    umountSmbData.insert(kNameKey, displayName);
    handleProtocolDeviceUnmounted(windowId, umountSmbData);
}

void SmbIntegrationManager::umountAllProtocolDevice(quint64 windowId, const QUrl &entryUrl, bool forgetPassword)
{
    // 1. umount all
    QStringList devs = DevProxyMng->getAllProtocolIds();

    for (const auto &devId : devs) {
        if (devId.startsWith(Global::Scheme::kSmb)) {
            QUrl smbUrl(devId);
            if (smbUrl.host() == entryUrl.host()) {
                DevMngIns->unmountProtocolDevAsync(devId, {}, [=](bool ok, DFMMOUNT::DeviceError err) {
                    if (!ok) {
                        qWarning() << "unmount protocol device failed: " << devId << err;
                        DialogManagerInstance->showErrorDialogWhenOperateDeviceFailed(DFMBASE_NAMESPACE::DialogManager::kUnmount, err);
                    }
                });
            }
        } else if (DeviceUtils::isSamba(QUrl(devId))) {
            QUrl compareUrl;
            compareUrl.setScheme(Global::Scheme::kFile);
            compareUrl.setPath(QString("/media/%1/smbmounts/").arg(SysInfoUtils::getUser()));
            if (devId.endsWith(entryUrl.host()) && devId.startsWith(compareUrl.toString())) {
                QUrl devUrl;
                devUrl.setScheme(Global::Scheme::kEntry);
                auto path = devId.toUtf8().toBase64();
                QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtocol);
                devUrl.setPath(encodecPath);
                DFMEntryFileInfoPointer info(new EntryFileInfo(devUrl));
                if (!info->exists())
                    continue;
                DevMngIns->unmountProtocolDevAsync(devId, {}, [=](bool ok, DFMMOUNT::DeviceError err) {
                    if (!ok) {
                        qWarning() << "unmount protocol device failed: " << devId << err;
                        DialogManagerInstance->showErrorDialogWhenOperateDeviceFailed(DFMBASE_NAMESPACE::DialogManager::kUnmount, err);
                    }
                });
            }
        }
    }

    // 2. clear password
    if (forgetPassword) {
        QUrl url;
        url.setScheme(Global::Scheme::kSmb);
        url.setHost(entryUrl.host());
        clearPasswd(url);
    }

    // 3. remove smb integration item (even if the `Keep showing the mounted Samba shares` is checked.)
    QUrl url;
    url.setScheme(Global::Scheme::kEntry);
    url.setHost(entryUrl.host());
    url.setPath(kSmbIntegrationPath);
    dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
    dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", entryUrl);

    QList<quint64> windowIds = FMWindowsIns.windowIdList();
    for (quint64 winId : windowIds) {
        auto window = FMWindowsIns.findWindowById(winId);
        if (!window)
            continue;
        const auto &curUrl = window->currentUrl();
        if (curUrl.scheme() == Global::Scheme::kSmb && curUrl.host() == url.host()) {
            QUrl computerUrl;
            computerUrl.setScheme(Global::Scheme::kComputer);
            SmbBrowserEventCaller::sendChangeCurrentUrl(winId, computerUrl);
        }
    }
}

bool SmbIntegrationManager::isSmbShareDirMounted(const QUrl &url)
{
    if (url.scheme() != Global::Scheme::kSmb)
        return false;

    QStringList devs = DevProxyMng->getAllProtocolIds();
    for (const QString &dev : devs) {
        if (dev.startsWith(Global::Scheme::kSmb)) {   // mounted by gvfs
            if (UniversalUtils::urlEquals(url, QUrl(dev)))
                return true;
        }
        if (DeviceUtils::isSamba(dev)) {   // mounted by cifs
            const QUrl &temUrl = QUrl::fromPercentEncoding(dev.toUtf8());
            const QString &path = temUrl.path();
            int pos = path.lastIndexOf("/");
            const QString &displayName = path.mid(pos + 1);
            if (QString("%1 on %2").arg(url.fileName()).arg(url.host()) == displayName)
                return true;
        }
    }

    return false;
}

/**
 * @brief SmbIntegrationManager::getSmbMountPathsByUrl
 * @param url : smb://1.2.3.4/sharedir, from workspace
 * @return gvfs and/or cifs mount path(s)
 */
QStringList SmbIntegrationManager::getSmbMountPathsByUrl(const QUrl &url)
{
    if (url.scheme() != Global::Scheme::kSmb)
        return QStringList();

    QStringList smbMountPaths;
    QStringList devs = DevProxyMng->getAllProtocolIds();
    for (const QString &dev : devs) {
        if (dev.startsWith(Global::Scheme::kSmb)) {
            if (dev == url.toString() + "/")   // dev like: smb://1.2.3.4/share_dir/
                smbMountPaths << dev;
        } else if (DeviceUtils::isSamba(dev)) {
            if (url.fileName() == QUrl(dev).fileName().section(" on ", 0, 0)) {
                if (url.host() == QUrl(dev).fileName().section(" on ", 1, 1))
                    smbMountPaths << dev;
            }
        }
    }

    return smbMountPaths;
}

void SmbIntegrationManager::actUmount(quint64 winId, const QStringList &smbMountPaths)
{
    windowId = winId;
    for (const QString &mountPos : smbMountPaths) {
        DevMngIns->unmountProtocolDevAsync(mountPos, {}, [=](bool ok, DFMMOUNT::DeviceError err) {
            if (!ok) {
                qWarning() << "unmount protocol device failed: " << mountPos << err;
                DialogManagerInstance->showErrorDialogWhenOperateDeviceFailed(DFMBASE_NAMESPACE::DialogManager::kUnmount, err);
            }
        });
    }
}

void SmbIntegrationManager::addIntegrationItemToSidebar(const QUrl &hostUrl, ContextMenuCallback contexMenu)
{
    if (hostUrl.scheme() != Global::Scheme::kSmb)
        return;

    ItemClickedActionCallback cdCb = [this](quint64 winId, const QUrl &url) { this->onOpenItem(winId, url); };
    FindMeCallback findMeCb = [](const QUrl &itemUrl, const QUrl &targetUrl) {
        QUrl hostUrl;
        hostUrl.setScheme(Global::Scheme::kSmb);   //change entry:// to smb://
        hostUrl.setHost(itemUrl.host());   //remove /.smbinteg
        return UniversalUtils::urlEquals(hostUrl, targetUrl);
    };
    Qt::ItemFlags flags { Qt::ItemIsEnabled | Qt::ItemIsSelectable };
    QVariantMap map {
        { "Property_Key_Group", "Group_Network" },
        { "Property_Key_DisplayName", hostUrl.host() },
        { "Property_Key_Icon", QIcon::fromTheme("folder-remote-symbolic") },
        { "Property_Key_QtItemFlags", QVariant::fromValue(flags) },
        { "Property_Key_CallbackItemClicked", QVariant::fromValue(cdCb) },
        { "Property_Key_CallbackContextMenu", QVariant::fromValue(contexMenu) },
        { "Property_Key_VisiableControl", "mounted_share_dirs" },
        { "Property_Key_CallbackFindMe", QVariant::fromValue(findMeCb) },
    };
    QUrl url;
    url.setScheme(Global::Scheme::kEntry);
    url.setHost(hostUrl.host());
    url.setPath(kSmbIntegrationPath);

    dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Add", url, map);
}

void SmbIntegrationManager::addStashedSeperatedItemToSidebar(QVariantMap stashedSeperatedData, ContextMenuCallback contexMenu)
{
    for (const QVariant &value : stashedSeperatedData) {
        ItemClickedActionCallback cdCb = [this](quint64 winId, const QUrl &url) { this->onOpenItem(winId, url); };
        Qt::ItemFlags flags { Qt::ItemIsEnabled | Qt::ItemIsSelectable };
        QVariantMap map {
            { "Property_Key_Group", kGroupNetwork },
            { "Property_Key_DisplayName", value.toString() },
            { "Property_Key_Icon", QIcon::fromTheme("folder-remote-symbolic") },
            { "Property_Key_QtItemFlags", QVariant::fromValue(flags) },
            { "Property_Key_CallbackItemClicked", QVariant::fromValue(cdCb) },
            { "Property_Key_CallbackContextMenu", QVariant::fromValue(contexMenu) },
            { "Property_Key_VisiableControl", "mounted_share_dirs" }
        };
        const QString &id = stashedSeperatedData.key(value);
        QUrl stashedUrl;
        stashedUrl.setScheme(Global::Scheme::kEntry);
        auto path = id.toUtf8().toBase64();
        QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtodevstashed);
        stashedUrl.setPath(encodecPath);
        // stashedUrl can be removed after mounting a new mount.
        dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Add", stashedUrl, map);
    }
}

void SmbIntegrationManager::addIntegrationItemToComputer(const QUrl &hostUrl)
{
    QUrl devUrl;
    devUrl.setScheme(Global::Scheme::kEntry);
    devUrl.setHost(hostUrl.host());
    devUrl.setPath(kSmbIntegrationPath);
    dpfSlotChannel->push("dfmplugin_computer", "slot_AddDevice", tr("Disks"), devUrl, 1, false);
}

void SmbIntegrationManager::doSwitchToSmbIntegratedMode(const QList<QUrl> &stashedUrls)
{
    // 1. remove the stashed separated smb mount icon from UI
    QStringList smbRootUrls;

    for (const QUrl &url : stashedUrls) {
        const QString &id = url.toString();
        QUrl stashedUrl;
        stashedUrl.setScheme(Global::Scheme::kEntry);
        auto path = id.toUtf8().toBase64();
        QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtodevstashed);
        stashedUrl.setPath(encodecPath);

        dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", stashedUrl);
        const QString &smbRoot = url.toString(QUrl::RemovePath);
        if (!smbRoot.isEmpty())
            smbRootUrls.append(smbRoot);
    }

    smbRootUrls.removeDuplicates();
    //2. Stash the smb integration to config
    if (!smbRootUrls.isEmpty()) {
        //`smbRootUrls` which is from field `RemoteMounts` is saved to field `SmbIntegrations` when switch to smb integration mode
        QStringList list = Application::genericSetting()->value(kStashedSmbDevices, kSmbIntegrations).toStringList();
        list.append(smbRootUrls);
        list.removeDuplicates();
        Application::genericSetting()->setValue(kStashedSmbDevices, kSmbIntegrations, list);
    }

    // 3. remove the mounted separated smb device from UI (just remove device icon, dont unmount)
    QStringList devs = DevProxyMng->getAllProtocolIds();
    for (const QString &id : devs) {
        QUrl mountedUrl;
        if (id.startsWith(Global::Scheme::kSmb)) {
            mountedUrl.setScheme(Global::Scheme::kEntry);
            auto path = id.toUtf8().toBase64();
            QString encodecPath = QString("%1.%2").arg(QString(path)).arg("protodev");
            mountedUrl.setPath(encodecPath);
        } else if (DeviceUtils::isSamba(QUrl(id))) {
            mountedUrl.setScheme(Global::Scheme::kEntry);
            auto path = id.toUtf8().toBase64();
            QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtocol);
            mountedUrl.setPath(encodecPath);
        }
        dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", mountedUrl);   // sidebar item would be removed together as well
    }

    isSmbIntegrated = true;
    // 4. refresh the UI to smb integration mode
    Q_EMIT refreshToSmbIntegrationMode(windowId);
}

void SmbIntegrationManager::doSwitchToSmbSeperatedMode(const QVariantMap &stashedSeperatedData, const QList<QUrl> &stashedUrls)
{
    // get stashed smb info from `StashedSmbDevices/SmbIntegrations` and save to `smbIntegrationUrls`.
    const QStringList &smbIntegrations = Application::genericSetting()->value(kStashedSmbDevices, kSmbIntegrations).toStringList();
    QList<QUrl> smbIntegrationUrls;   // list format: smb://1.2.3.4
    for (const QString &smbIntegration : smbIntegrations)
        smbIntegrationUrls.append(QUrl(smbIntegration));

    // 1. remove all the smb integrations from UI (do not unmount)
    for (const QUrl &unmount : smbIntegrationUrls) {
        QUrl url;
        url.setScheme(Global::Scheme::kEntry);
        url.setHost(unmount.host());
        url.setPath(kSmbIntegrationPath);
        dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
        dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", url);
    }

    // 2. unmount all the mounted separated smb mount
    QStringList devs = DevProxyMng->getAllProtocolIds();
    QStringList unmountList;
    auto makeUnmountEntryUrl = [](const QString &host) {
        QUrl hostUrl;
        hostUrl.setScheme(Global::Scheme::kEntry);
        hostUrl.setHost(host);
        hostUrl.setPath(kSmbIntegrationPath);
        return hostUrl;
    };
    for (const QString &dev : devs) {
        if (dev.startsWith(Global::Scheme::kSmb)) {
            QUrl hostUrl = makeUnmountEntryUrl(QUrl(dev).host());
            unmountList << hostUrl.toString();
        } else if (DeviceUtils::isSamba(dev)) {
            const QUrl &url = QUrl::fromPercentEncoding(dev.toUtf8());
            const QString &path = url.path();
            int pos = path.lastIndexOf("/");
            const QString &displayName = path.mid(pos + 1);
            const QString &host = displayName.section("on", 1, 1).trimmed();
            QUrl hostUrl = makeUnmountEntryUrl(host);
            unmountList << hostUrl.toString();
        }
    }
    unmountList.removeDuplicates();
    for (const QString &id : unmountList) {
        umountAllProtocolDevice(windowId, QUrl(id), true);
    }

    // 3. Add stashed separated smb mount by refresh UI
    if (isStashMountsEnabled()) {
        isSmbIntegrated = false;
        Q_EMIT refreshToSmbSeperatedMode(stashedSeperatedData, stashedUrls);
    }

    // 4. Switch to computer view
    QUrl computerUrl;
    computerUrl.setScheme(Global::Scheme::kComputer);
    SmbBrowserEventCaller::sendChangeCurrentUrl(windowId, computerUrl);
}

/**
 * @brief SmbIntegrationManager::smbMountExists
 * To check whether exist smb mount under host
 * @param host
 * @return exist or not exist smb mount under host
 */
bool SmbIntegrationManager::smbMountExists(const QString &host)
{
    QStringList devs = DevProxyMng->getAllProtocolIds();

    for (const QString &dev : devs) {
        if (dev.startsWith(Global::Scheme::kSmb)) {
            if (QUrl(dev).host() == host)   // dev like: smb://1.2.3.4/share_dir/
                return true;
        } else if (DeviceUtils::isSamba(dev)) {
            const QUrl &url = QUrl::fromPercentEncoding(dev.toUtf8());
            const QString &path = url.path();
            int pos = path.lastIndexOf("/");
            const QString &displayName = path.mid(pos + 1);
            const QString &temHost = displayName.section("on", 1, 1).trimmed();
            if (temHost == host)
                return true;
        }
    }

    return false;
}

void SmbIntegrationManager::stashSmbMount(const QString &id)
{
    if (!isStashMountsEnabled()) {
        qDebug() << "stash mounts is disabled";
        return;
    }
    QVariantHash newMount;
    QString host;
    QString shareName;
    QString displayName;
    if (DeviceUtils::isSamba(QUrl(id))) {
        const QUrl &url = QUrl::fromPercentEncoding(id.toUtf8());
        const QString &path = url.path();
        int pos = path.lastIndexOf("/");
        displayName = path.mid(pos + 1);
        host = displayName.section(" on ", 1, 1);
        shareName = displayName.section(" on ", 0, 0);
    } else if (id.startsWith(Global::Scheme::kSmb)) {
        host = QUrl(id).host();
        shareName = QUrl(id.endsWith("/") ? id.chopped(1) : id).fileName();
        displayName = QString("%1 on %2").arg(shareName).arg(host);
    } else {
        return;
    }
    if (host.isEmpty() || shareName.isEmpty() || displayName.isEmpty())
        return;
    newMount.insert(kHostKey, host);
    newMount.insert(kShareKey, shareName);
    newMount.insert(kProtocolKey, Global::Scheme::kSmb);
    newMount.insert(kNameKey, displayName);
    const QString &key = QString("%1/smb-share:server=%2,share=%3")
                                 .arg(SysInfoUtils::isRootUser()
                                              ? QString("/root/.gvfs")
                                              : QString("/run/user/%1/gvfs").arg(SysInfoUtils::getUserId()))
                                 .arg(newMount.value(kHostKey).toString())
                                 .arg(newMount.value(kShareKey).toString());

    Application::genericSetting()->setValue(kRemoteMounts, key, newMount);
}

void SmbIntegrationManager::clearPasswd(const QUrl &url)
{
    QString server = url.host();
    QString protocol = url.scheme();

    if (protocol == DFMBASE_NAMESPACE::Global::Scheme::kSmb) {
        secret_password_clear(smbSchema(), nullptr, onPasswdCleared, nullptr,
                              // "user", user.toStdString().c_str(),
                              // "domain", domain.toStdString().c_str(),
                              "server", server.toStdString().c_str(),
                              "protocol", protocol.toStdString().c_str(),
                              nullptr);
    } else if (protocol.endsWith(DFMBASE_NAMESPACE::Global::Scheme::kFtp)) {   // ftp && sftp
        secret_password_clear(ftpSchema(), nullptr, onPasswdCleared, nullptr,
                              // "user", user.toStdString().c_str(),
                              "server", server.toStdString().c_str(),
                              "protocol", protocol.toStdString().c_str(),
                              nullptr);
    }
}

QString SmbIntegrationManager::parseServer(const QString &uri)
{
    // AppController::actionForgetPassword
    QStringList frags = uri.split("/");
    if (frags.count() < 3)
        return "";

    // just trans from old version. there is no example and no annotation...
    QString authField = frags.at(2);
    if (authField.contains(";")) {
        QStringList authFrags = authField.split(";");
        if (authFrags.count() >= 2) {
            QString userAuth = authFrags.at(1);
            if (userAuth.contains("@")) {
                QStringList userAuthFrags = userAuth.split("@");
                if (userAuthFrags.count() >= 2)
                    return userAuthFrags.at(1);
            }
        }
    } else {
        if (authField.contains("@")) {
            QStringList authFrags = authField.split("@");
            if (authFrags.count() >= 2)
                return authFrags.at(1);
        } else {
            return authField;
        }
    }

    return "";
}

void SmbIntegrationManager::addStashedSeperatedItemToComputer(const QList<QUrl> &urlList)
{
    for (const QUrl &url : urlList) {
        const QString &id = url.toString();
        QUrl stashedUrl;
        stashedUrl.setScheme(Global::Scheme::kEntry);
        auto path = id.toUtf8().toBase64();
        QString encodecPath = QString("%1.%2").arg(QString(path)).arg(kProtodevstashed);
        stashedUrl.setPath(encodecPath);

        dpfSlotChannel->push("dfmplugin_computer", "slot_AddDevice", tr("Disks"), stashedUrl, 1, false);
    }
}

bool SmbIntegrationManager::handleItemListFilter(QList<QUrl> *items)
{
    static QString smbMatch { "(^/run/user/\\d+/gvfs/smb|^/root/\\.gvfs/smb|^/media/[\\s\\S]*/smbmounts)" };   // TODO(xust) /media/$USER/smbmounts might be changed in the future.}
    auto isSamba = [](const QString &path, const QString &smbMatch) {
        QRegularExpression re(smbMatch);
        QRegularExpressionMatch match = re.match(path);
        return match.hasMatch();
    };
    QList<QUrl> removeList;
    for (const QUrl &url : *items) {
        if (isSmbIntegrated) {
            if (url.path().endsWith(kProtodevstashed)) {
                QString suffix = QString(".%1").arg(kProtodevstashed);
                QString encodecId = url.path().remove(suffix);
                const QString &id = QByteArray::fromBase64(encodecId.toUtf8());
                if (id.startsWith(Global::Scheme::kSmb)) {
                    removeList << url;
                }
            }
            if (url.path().endsWith(kProtocol)) {
                QString suffix = QString(".%1").arg(kProtocol);
                QString encodecId = url.path().remove(suffix);
                const QString &id = QByteArray::fromBase64(encodecId.toUtf8());
                if (id.startsWith(Global::Scheme::kSmb) || isSamba(QUrl(id).path(), smbMatch)) {
                    removeList << url;
                }
            }
        } else {
            if (url.path().endsWith(kSmbIntegrationSuffix))
                removeList << url;
        }
    }
    while (!removeList.isEmpty()) {
        QUrl url = removeList.takeFirst();
        dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
        items->removeOne(url);
    }
    return true;
}

bool SmbIntegrationManager::handleItemFilterOnAdd(const QUrl &devUrl)
{
    static QString smbMatch { "(^/run/user/\\d+/gvfs/smb|^/root/\\.gvfs/smb|^/media/[\\s\\S]*/smbmounts)" };   // TODO(xust) /media/$USER/smbmounts might be changed in the future.}
    auto isSamba = [](const QString &path, const QString &smbMatch) {
        QRegularExpression re(smbMatch);
        QRegularExpressionMatch match = re.match(path);
        return match.hasMatch();
    };
    DFMEntryFileInfoPointer info(new EntryFileInfo(devUrl));
    if (!info->exists())
        return false;

    if (isSmbIntegrated) {
        if (devUrl.path() == kSmbIntegrationPath)
            return false;

        if (info->nameOf(AbstractFileInfo::FileNameInfoType::kSuffix) == kProtocol) {
            QUrl url = info->urlOf(AbstractFileInfo::FileUrlInfoType::kUrl);
            QString suffix = QString(".%1").arg(kProtocol);
            QString encodecId = url.path().remove(suffix);
            QString id = QByteArray::fromBase64(encodecId.toUtf8());
            if (id.startsWith(Global::Scheme::kSmb) || isSamba(QUrl(id).path(), smbMatch)) {
                dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
                return true;
            }
        }

        if (info->nameOf(AbstractFileInfo::FileNameInfoType::kSuffix) == kProtodevstashed) {
            QUrl url = info->urlOf(AbstractFileInfo::FileUrlInfoType::kUrl);
            QString suffix = QString(".%1").arg(kProtodevstashed);
            QString encodecId = url.path().remove(suffix);
            QString id = QByteArray::fromBase64(encodecId.toUtf8());
            if (id.startsWith(Global::Scheme::kSmb)) {
                dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
                return true;
            }
        }
    } else {
        if (info->nameOf(AbstractFileInfo::FileNameInfoType::kSuffix) == kProtocol) {
            QUrl protocalUrl = info->urlOf(AbstractFileInfo::FileUrlInfoType::kUrl);
            QString suffix = QString(".%1").arg(kProtocol);
            QString encodecId = protocalUrl.path().remove(suffix);
            QString id = QByteArray::fromBase64(encodecId.toUtf8());

            stashSmbMount(id);   // add smb stashed info to `RemoteMounts` of config, but here still saved as V1 format
        }
    }

    return false;
}

bool SmbIntegrationManager::handleItemFilterOnRemove(const QUrl &devUrl)
{
    if (!devUrl.path().endsWith(kProtocol))
        return false;

    const QString &suffix = QString(".%1").arg(kProtocol);
    const QString encodecId = devUrl.path().remove(suffix);
    const QString &id = QByteArray::fromBase64(encodecId.toUtf8());
    QString smbHost;
    QString shareName;
    if (DeviceUtils::isSamba(QUrl(id))) {
        const QUrl &temUrl = QUrl::fromPercentEncoding(id.toUtf8());
        const QString &path = temUrl.path();
        int pos = path.lastIndexOf("/");
        const QString &displayName = path.mid(pos + 1);
        smbHost = displayName.section(" on ", 1, 1);
        shareName = displayName.section(" on ", 0, 0);
    } else if (id.startsWith(Global::Scheme::kSmb)) {
        smbHost = QUrl(id).host();
        shareName = QUrl(id.chopped(1)).fileName();
    } else {
        return false;
    }

    if (smbHost.isEmpty() || shareName.isEmpty())
        return false;
    bool re = smbMountExists(smbHost);
    if (!re) {   //When all smb share folders are unmounted
        QVariantHash pwTypeData = Application::genericSetting()->value(kStashedSmbDevices, kSavedPasswordType, QVariantHash()).toHash();
        QUrl hostUrl;
        hostUrl.setScheme(Global::Scheme::kSmb);
        hostUrl.setHost(smbHost);
        const QString &key = hostUrl.toString();
        int savedPwType = pwTypeData.value(key).toInt();
        //1. remove password according to the password type
        bool needForgetPassword = savedPwType != 2;   // 2 means: the password saving type is `NetworkMountPasswdSaveMode::kSavePermanently`
        if (needForgetPassword) {
            // if user did not check `save password` when mount smb, `NetworkMountPasswdSaveMode::kSaveBeforeLogouthere` is used as default,
            // so here need to forget this kind of password when all the smb mounts be unmounted.
            QUrl clearPasswordUrl;
            clearPasswordUrl.setScheme(Global::Scheme::kSmb);
            clearPasswordUrl.setHost(smbHost);
            clearPasswordUrl.setPath("/" + shareName + "/");
            this->clearPasswd(clearPasswordUrl);
            pwTypeData.remove(key);
            Application::genericSetting()->setValue(kStashedSmbDevices, kSavedPasswordType, pwTypeData);
        }
        //2. if isSmbStashMountsEnabled = false, and all the smb shared folders are unmounted, then
        //remove the smb ingtegration item from sidebar and computer view
        if (!isSmbStashMountsEnabled) {
            QUrl url;
            url.setScheme(Global::Scheme::kEntry);
            url.setHost(smbHost);
            url.setPath(kSmbIntegrationPath);
            dpfSlotChannel->push("dfmplugin_sidebar", "slot_Item_Remove", url);
            dpfSlotChannel->push("dfmplugin_computer", "slot_RemoveDevice", url);
            url.setScheme(Global::Scheme::kSmb);
            url.setHost(smbHost);
            SmbIntegrationManager::instance()->removeStashedIntegrationFromConfig(url.toString());
            QUrl computerUrl;
            computerUrl.setScheme(Global::Scheme::kComputer);
            SmbBrowserEventCaller::sendChangeCurrentUrl(windowId, computerUrl);
        }
    }

    return false;
}

const SecretSchema *SmbIntegrationManager::smbSchema()
{
    static const SecretSchema schema {
        "org.gnome.keyring.NetworkPassword",   // name
        SECRET_SCHEMA_DONT_MATCH_NAME,   // flag
        { { "user", SECRET_SCHEMA_ATTRIBUTE_STRING },   // attrs
          { "domain", SECRET_SCHEMA_ATTRIBUTE_STRING },
          { "server", SECRET_SCHEMA_ATTRIBUTE_STRING },
          { "protocol", SECRET_SCHEMA_ATTRIBUTE_STRING } },
        0,   // reserved
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    return &schema;
}

const SecretSchema *SmbIntegrationManager::ftpSchema()
{
    static const SecretSchema schema {
        "org.gnome.keyring.NetworkPassword",   // name
        SECRET_SCHEMA_DONT_MATCH_NAME,   // flag
        { { "user", SECRET_SCHEMA_ATTRIBUTE_STRING },   // attrs
          { "server", SECRET_SCHEMA_ATTRIBUTE_STRING },
          { "protocol", SECRET_SCHEMA_ATTRIBUTE_STRING } },
        0,   // reserved
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };
    return &schema;
}

void SmbIntegrationManager::onPasswdCleared(GObject *obj, GAsyncResult *res, gpointer data)
{
    Q_UNUSED(obj)
    Q_UNUSED(data)

    GError_autoptr err = nullptr;
    bool ret = secret_password_clear_finish(res, &err);
    qDebug() << "on password cleared: " << ret;
    if (err)
        qInfo() << "error while clear saved password: " << err->message;
}
