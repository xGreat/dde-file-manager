// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dfm_global_defines.h"
#include "protocolentryfileentity.h"
#include "utils/computerdatastruct.h"
#include "utils/computerutils.h"

#include "dfm-base/file/entry/entryfileinfo.h"
#include "dfm-base/dbusservice/global_server_defines.h"
#include "dfm-base/base/urlroute.h"
#include "dfm-base/base/device/deviceproxymanager.h"
#include "dfm-base/base/device/deviceutils.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/dfm_global_defines.h"

using namespace dfmplugin_computer;
using namespace GlobalServerDefines;

/*!
 * \class ProtocolEntryFileEntity
 * \brief class that present protocol devices
 */
ProtocolEntryFileEntity::ProtocolEntryFileEntity(const QUrl &url)
    : AbstractEntryFileEntity(url)
{
    if (!url.path().endsWith(SuffixInfo::kProtocol)) {
        qWarning() << "wrong suffix in" << __FUNCTION__ << "url";
        abort();
    }

    refresh();
}

QString ProtocolEntryFileEntity::displayName() const
{
    QString displayName = datas.value(DeviceProperty::kDisplayName).toString();
    if (displayName.contains(" on ")) {
        int pos = displayName.lastIndexOf(" on ");
        const QString &shareName = displayName.left(pos);
        const QString &hostName = displayName.right(displayName.length() - pos - 4);
        if (!shareName.isEmpty() && !hostName.isEmpty())
            displayName = QString("%1 %2 %3").arg(shareName).arg(QObject::tr("on")).arg(hostName);
    }

    return displayName;
}

QIcon ProtocolEntryFileEntity::icon() const
{
    auto icons = datas.value(DeviceProperty::kDeviceIcon).toStringList();
    for (const auto &icon : icons) {
        auto iconObj = QIcon::fromTheme(icon);
        if (iconObj.isNull())
            continue;
        return iconObj;
    }
    return {};
}

bool ProtocolEntryFileEntity::exists() const
{
    return !datas.value(DeviceProperty::kMountPoint).toString().isEmpty();
}

bool ProtocolEntryFileEntity::showProgress() const
{
    return true;
}

bool ProtocolEntryFileEntity::showTotalSize() const
{
    return true;
}

bool ProtocolEntryFileEntity::showUsageSize() const
{
    return true;
}

DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder ProtocolEntryFileEntity::order() const
{
    const QString &id = datas.value(DeviceProperty::kId).toString();

    if (id.startsWith(DFMBASE_NAMESPACE::Global::Scheme::kFtp)
        || id.startsWith(DFMBASE_NAMESPACE::Global::Scheme::kSFtp))
        return DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder::kOrderFtp;

    if (id.startsWith(DFMBASE_NAMESPACE::Global::Scheme::kSmb)
        || DFMBASE_NAMESPACE::DeviceUtils::isSamba(QUrl(id)))
        return DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder::kOrderSmb;

    if (id.startsWith(DFMBASE_NAMESPACE::Global::Scheme::kMtp))
        return DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder::kOrderMTP;

    if (id.startsWith(DFMBASE_NAMESPACE::Global::Scheme::kGPhoto2))
        return DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder::kOrderGPhoto2;

    return DFMBASE_NAMESPACE::EntryFileInfo::EntryOrder::kOrderFiles;
}

quint64 ProtocolEntryFileEntity::sizeTotal() const
{
    return datas.value(DeviceProperty::kSizeTotal).toULongLong();
}

quint64 ProtocolEntryFileEntity::sizeUsage() const
{
    return datas.value(DeviceProperty::kSizeUsed).toULongLong();
}

void ProtocolEntryFileEntity::refresh()
{
    auto encodecId = entryUrl.path().remove("." + QString(SuffixInfo::kProtocol)).toUtf8();
    auto id = QString(QByteArray::fromBase64(encodecId));

    datas = DFMBASE_NAMESPACE::UniversalUtils::convertFromQMap(DevProxyMng->queryProtocolInfo(id));
}

QUrl ProtocolEntryFileEntity::targetUrl() const
{
    auto mpt = datas.value(DeviceProperty::kMountPoint).toString();
    QUrl target;
    if (mpt.isEmpty())
        return target;
    target.setScheme(DFMBASE_NAMESPACE::Global::Scheme::kFile);
    target.setPath(mpt);
    return target;
}
