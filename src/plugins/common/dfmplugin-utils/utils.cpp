// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"
#include "reportlog/virtualreportlogplugin.h"
#include "global/virtualglobalplugin.h"
#include "openwith/virtualopenwithplugin.h"
#include "appendcompress/virtualappendcompressplugin.h"
#include "bluetooth/virtualbluetoothplugin.h"
#include "extensionimpl/virtualextensionimplplugin.h"
#include "vaultassist/virtualvaulthelperplugin.h"

using namespace dfmplugin_utils;

static constexpr char kReportLog[] { "dfmplugin-reportlog" };
static constexpr char kAppendcompress[] { "dfmplugin-appendcompress" };
static constexpr char kBluetooth[] { "dfmplugin-bluetooth" };
static constexpr char kGlobal[] { "dfmplugin-global" };
static constexpr char kOpenWith[] { "dfmplugin-openwith" };
static constexpr char kExtensionImpl[] { "dfmplugin-extensionimpl" };
static constexpr char kVaultAssist[] { "dfmplugin-vaultassist" };

QSharedPointer<DPF_NAMESPACE::Plugin> Utils::create(const QString &pluginName)
{
    qInfo() << "Create plugin: " << pluginName;

    if (pluginName == kReportLog) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualReportLogPlugin };
        return plugin;
    }

    if (pluginName == kAppendcompress) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualAppendCompressPlugin };
        return plugin;
    }

    if (pluginName == kBluetooth) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualBluetoothPlugin };
        return plugin;
    }

    if (pluginName == kGlobal) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualGlobalPlugin };
        return plugin;
    }

    if (pluginName == kOpenWith) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualOpenWithPlugin };
        return plugin;
    }

    if (pluginName == kExtensionImpl) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualExtensionImplPlugin };
        return plugin;
    }

    if (pluginName == kVaultAssist) {
        QSharedPointer<DPF_NAMESPACE::Plugin> plugin { new VirtualVaultHelperPlugin };
        return plugin;
    }

    return {};
}
