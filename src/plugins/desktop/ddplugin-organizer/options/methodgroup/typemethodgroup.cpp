// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "typemethodgroup.h"
#include "config/configpresenter.h"
#include "utils/organizerutils.h"

using namespace ddplugin_organizer;

namespace
{
    inline const char kKeyCheckboxID[] = "CheckboxID";
}

TypeMethodGroup::TypeMethodGroup(QObject *parent)
    : MethodGroupHelper(parent)
{
    QHash<ItemCategory, QString> *namePtr = const_cast<QHash<ItemCategory, QString> *>(&categoryName);
    *namePtr = {
            {kCatApplication, tr("Apps")},
            {kCatDocument, tr("Documents")},
            {kCatPicture, tr("Pictures")},
            {kCatVideo, tr("Videos")},
            {kCatMusic, tr("Music")},
            {kCatFloder, tr("Folders")}
        };
}

Classifier TypeMethodGroup::id() const
{
    return kType;
}

void TypeMethodGroup::release()
{
    for (auto wid : categories)
        delete wid;

    categories.clear();
}

bool TypeMethodGroup::build()
{
    if (!categories.isEmpty())
        return true;

    auto items = CfgPresenter->enabledTypeCategories();
    bool all = OrganizerUtils::isAllItemCategory(items);
    for (int i = kCatApplication; i <= kCatEnd; i = i << 1) {
        auto check = new CheckBoxWidget(categoryName.value(static_cast<ItemCategory>(i)));
        check->setProperty(kKeyCheckboxID, i);
        connect(check, &CheckBoxWidget::chenged, this, &TypeMethodGroup::onChenged);

        check->setChecked(all ? true : items & i);
        categories.append(check);
    }

    return true;
}

QList<QWidget*> TypeMethodGroup::subWidgets() const
{
    QList<QWidget*> wids;
    for (QWidget *w : categories)
        wids << w;
    return wids;
}

void TypeMethodGroup::onChenged(bool on)
{
    if (auto box = qobject_cast<CheckBoxWidget *>(sender())) {
        QVariant var = box->property(kKeyCheckboxID);
        if (var.isValid()) {
            auto value = var.toInt();
            if (value > kCatNone && value <= kCatEnd) {
                auto flag = static_cast<ItemCategory>(value);
                auto flags = CfgPresenter->enabledTypeCategories();
                //! the flags may be -1 that can not to perform bit operation.
                //! so we need to cover it to kCatAll.
                if (OrganizerUtils::isAllItemCategory(flags))
                    flags = kCatAll;

                bool apply = false;
                if (on) {
                    if (!flags.testFlag(flag)) {
                        flags |= flag;
                        apply = true;
                    }
                } else {
                    if (flags.testFlag(flag)) {
                        flags &= ~flag;
                        apply = true;
                    }
                }

                if (apply) {
                    if (OrganizerUtils::isAllItemCategory(flags))
                        flags = kCatDefault;

                    CfgPresenter->setEnabledTypeCategories(flags);
                    // using switch signal to reset the collection.
                    emit CfgPresenter->switchToNormalized(id());
                }
            }
        }
    }
}
