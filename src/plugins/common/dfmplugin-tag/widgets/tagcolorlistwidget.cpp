// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tagcolorlistwidget.h"
#include "tagbutton.h"
#include "utils/taghelper.h"

#include <QLabel>
#include <QVBoxLayout>

using namespace dfmplugin_tag;

TagColorListWidget::TagColorListWidget(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("tagActionWidget");
    setFocusPolicy(Qt::StrongFocus);

    setCentralLayout();
    initUiElement();
    initConnect();
}

QList<QColor> TagColorListWidget::checkedColorList() const
{
    QList<QColor> list;

    for (const TagButton *button : tagButtons) {
        if (button->isChecked())
            list << button->color();
    }

    return list;
}

void TagColorListWidget::setCheckedColorList(const QList<QColor> &colorNames)
{
    for (TagButton *button : tagButtons) {
        button->setChecked(colorNames.contains(button->color()));
    }
}

bool TagColorListWidget::exclusive() const
{
    return currentExclusive;
}

void TagColorListWidget::setExclusive(bool exclusive)
{
    currentExclusive = exclusive;
}

void TagColorListWidget::setToolTipVisible(bool visible)
{
    if (toolTip)
        toolTip->setVisible(visible);
}

void TagColorListWidget::setToolTipText(const QString &text)
{
    if (toolTip && toolTip->isVisible())
        toolTip->setText(text);
}

void TagColorListWidget::clearToolTipText()
{
    setToolTipText(QStringLiteral(" "));
}

void TagColorListWidget::setCentralLayout() noexcept
{
    setLayout(mainLayout);
}

void TagColorListWidget::initUiElement()
{
    QList<QColor> colors = TagHelper::instance()->defualtColors();

    for (const QColor &color : colors) {
        tagButtons << new TagButton(color, this);
    }

    buttonLayout = new QHBoxLayout;

    buttonLayout->addSpacing(21);
    for (int index = 0; index < tagButtons.length(); ++index) {
        tagButtons[index]->setContentsMargins(0, 0, 0, 0);
        tagButtons[index]->setRadius(20);

        QString objMark = QString("Color%1").arg(index + 1);
        tagButtons[index]->setObjectName(objMark);

        buttonLayout->addWidget(tagButtons[index], Qt::AlignCenter);
    }

    buttonLayout->addSpacing(21);
    buttonLayout->setMargin(0);
    buttonLayout->setSpacing(0);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(buttonLayout);

    toolTip = new QLabel(this);
    toolTip->setText(QStringLiteral(" "));
    toolTip->setStyleSheet("color: #707070; font-size: 10px");
    toolTip->setObjectName("tool_tip");

    mainLayout->addWidget(toolTip, 0, Qt::AlignHCenter);
}

void TagColorListWidget::initConnect()
{
    for (TagButton *button : tagButtons) {
        connect(button, &TagButton::enter, this, [this, button] {
            emit hoverColorChanged(button->color());
        });

        connect(button, &TagButton::leave, this, [this] {
            emit hoverColorChanged(QColor());
        });

        connect(button, &TagButton::checkedChanged, this, [this, button] {
            if (button->isChecked() && exclusive()) {
                for (TagButton *b : tagButtons) {
                    if (b != button)
                        b->setChecked(false);
                }
            }
        });

        connect(button, &TagButton::click, this, [this](QColor color) {
            emit checkedColorChanged(color);
        });
    }
}
