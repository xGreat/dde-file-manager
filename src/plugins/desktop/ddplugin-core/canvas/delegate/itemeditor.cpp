/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangyu<zhangyub@uniontech.com>
 *
 * Maintainer: zhangyu<zhangyub@uniontech.com>
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
#include "itemeditor.h"
#include "canvasitemdelegate.h"

#include "dfm-base/utils/fileutils.h"

#include <DArrowRectangle>

#include <QApplication>
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QLabel>

DWIDGET_USE_NAMESPACE
DSB_D_USE_NAMESPACE

ItemEditor::ItemEditor(QWidget *parent) : QFrame(parent)
{
    init();
}

ItemEditor::~ItemEditor()
{
    if (tooltip) {
        tooltip->hide();
        tooltip->deleteLater();
        tooltip = nullptr;
    }
}

void ItemEditor::setBaseGeometry(const QRect &base, const QSize &itemSize, const QMargins &margin)
{
    delete layout();

    // move editor to the grid.
    move(base.topLeft());
    setFixedWidth(base.width());
    // minimum height is virtualRect's height.
    setMinimumHeight(base.height());

    auto lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->setSpacing(0);

    // add grid margin ,icon height ,icon space, text padding
    lay->setContentsMargins(margin);

    // the textEditor is on text painting area.
    lay->addWidget(textEditor, 0, Qt::AlignTop | Qt::AlignHCenter);

    itemSizeHint = itemSize;
    updateGeometry();
}

void ItemEditor::init()
{
    setFrameShape(QFrame::NoFrame);
    setContentsMargins(0, 0, 0, 0);
    textEditor = createEditor();
    textEditor->setParent(this);
    connect(textEditor, &RenameEdit::textChanged, this, &ItemEditor::textChanged, Qt::UniqueConnection);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setMargin(0);
    lay->setSpacing(0);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(textEditor, Qt::AlignTop | Qt::AlignHCenter);

    // the editor is real focus widget.
    setFocusProxy(textEditor);
}

void ItemEditor::updateGeometry()
{
    //! the textEditor->document()->textWidth will changed after textEditor->setFixedWidth although it was setted before.
    textEditor->setFixedWidth(width());

    //! so need to reset it.
    textEditor->document()->setTextWidth(itemSizeHint.width());

    int textHeight = static_cast<int>(textEditor->document()->size().height());
    if (textEditor->isReadOnly()) {
        textEditor->setFixedHeight(textHeight);
    } else {
        textHeight = qMin(fontMetrics().height() * 3 + CanvasItemDelegate::kTextPadding * 2, textHeight);
        textEditor->setFixedHeight(textHeight);
    }

    // make layout to adjust height.
    adjustSize();
    QFrame::updateGeometry();
}

void ItemEditor::showAlertMessage(const QString &text, int duration)
{
    if (!tooltip) {
        tooltip = createTooltip();
        tooltip->setBackgroundColor(palette().color(backgroundRole()));
        QTimer::singleShot(duration, this, [this] {
            if (tooltip) {
                tooltip->hide();
                tooltip->deleteLater();
                tooltip = nullptr;
            }
        });
    }

    if (QLabel *label = qobject_cast<QLabel *>(tooltip->getContent())) {
        label->setText(text);
        label->adjustSize();
    }

    QPoint pos = textEditor->mapToGlobal(QPoint(textEditor->width() / 2, textEditor->height()));
    tooltip->show(pos.x(), pos.y());
}

DArrowRectangle *ItemEditor::createTooltip()
{
    auto tooltip = new DArrowRectangle(DArrowRectangle::ArrowTop);
    tooltip->setObjectName("AlertTooltip");

    QLabel *label = new QLabel(tooltip);

    label->setWordWrap(true);
    label->setMaximumWidth(500);
    tooltip->setContent(label);
    tooltip->setArrowX(15);
    tooltip->setArrowHeight(5);
    return tooltip;
}

QString ItemEditor::text() const
{
    return textEditor->toPlainText();
}

void ItemEditor::setText(const QString &text)
{
    textEditor->setPlainText(text);
    textEditor->setAlignment(Qt::AlignHCenter);
    updateGeometry();
}

void ItemEditor::setItemSizeHint(QSize size)
{
    itemSizeHint = size;
    updateGeometry();
}

void ItemEditor::select(const QString &part)
{
    QString org = text();
    if (org.contains(part)) {
        int start = org.indexOf(org);
        if (Q_UNLIKELY(start < 0))
            start = 0;
        int end = start + part.size();
        if (Q_UNLIKELY(end > org.size()))
            end = org.size();

        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        textEditor->setTextCursor(cursor);
    }
}

void ItemEditor::setOpacity(qreal opacity)
{
    if (opacity - 1.0 >= 0) {
        if (opacityEffect) {
            opacityEffect->deleteLater();
            opacityEffect = Q_NULLPTR;
        }
        return;
    }

    if (!opacityEffect) {
        opacityEffect = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(opacityEffect);
    }

    opacityEffect->setOpacity(opacity);
}

RenameEdit *ItemEditor::createEditor()
{
    auto edit = new RenameEdit();
    edit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    edit->setAlignment(Qt::AlignHCenter);
    edit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    edit->setFrameShape(QFrame::NoFrame);
    edit->setAcceptRichText(false);
    edit->setAcceptDrops(false);

    return edit;
}

bool ItemEditor::processLength(const QString &srcText, int srcPos, QString &dstText, int &dstPos)
{
    int editTextCurrLen = srcText.toLocal8Bit().size();
    int editTextRangeOutLen = editTextCurrLen - maximumLength();
    if (editTextRangeOutLen > 0 && maximumLength() != INT_MAX) {
        QVector<uint> list = srcText.toUcs4();
        QString tmp = srcText;
        while (tmp.toLocal8Bit().size() > maximumLength() && srcPos > 0) {
            list.removeAt(--srcPos);
            tmp = QString::fromUcs4(list.data(), list.size());
        }
        dstText = tmp;
        dstPos = srcPos;
        return srcText.size() != dstText.size();
    } else {
        dstText = srcText;
        dstPos = srcPos;
        return false;
    }
}

void ItemEditor::textChanged()
{
    if (sender() != textEditor)
        return;

    if (textEditor->isReadOnly())
        return;

    // block signal to prevent signal from being sent again.
    QSignalBlocker blocker(textEditor);

    const QString curText = textEditor->toPlainText();

    if (curText.isEmpty()) {
        blocker.unblock();
        updateGeometry();
        return;
    }

    QString dstText = DFMBASE_NAMESPACE::FileUtils::preprocessingFileName(curText);
    bool hasInvalidChar = dstText.size() != curText.size();
    int endPos = textEditor->textCursor().position() + (dstText.length() - curText.length());

    processLength(dstText, endPos, dstText, endPos);
    if (curText != dstText) {
        textEditor->setPlainText(dstText);
        QTextCursor cursor = textEditor->textCursor();
        cursor.setPosition(endPos);
        textEditor->setTextCursor(cursor);
        textEditor->setAlignment(Qt::AlignHCenter);
    }

    // push stack.
    if (textEditor->stackCurrent() != dstText)
        textEditor->pushStatck(dstText);

    blocker.unblock();
    updateGeometry();

    if (hasInvalidChar)
        showAlertMessage(tr("%1 are not allowed").arg("|/\\*:\"'?<>"));
}

void RenameEdit::undo()
{
    // make signal textChanged to be invalid to push stack.
    enableStack = false;

    QTextCursor cursor = textCursor();
    setPlainText(stackBack());
    setTextCursor(cursor);
    setAlignment(Qt::AlignHCenter);
    enableStack = true;

    // update edit height by ItemEditor::updateGeometry
    QMetaObject::invokeMethod(parent(), "updateGeometry");
}

void RenameEdit::redo()
{
    // make signal textChanged to be invalid.
    enableStack = false;

    QTextCursor cursor = textCursor();
    setPlainText(stackAdvance());
    setTextCursor(cursor);
    setAlignment(Qt::AlignHCenter);
    enableStack = true;

    // update edit height by ItemEditor::updateGeometry
    QMetaObject::invokeMethod(parent(), "updateGeometry");
}

QString RenameEdit::stackCurrent() const
{
    return textStack.value(stackCurrentIndex);
}

QString RenameEdit::stackBack()
{
    stackCurrentIndex = qMax(0, stackCurrentIndex - 1);
    const QString &text = stackCurrent();
    return text;
}

QString RenameEdit::stackAdvance()
{
    stackCurrentIndex = qMin(textStack.count() - 1, stackCurrentIndex + 1);
    const QString &text = stackCurrent();
    return text;
}

void RenameEdit::pushStatck(const QString &item)
{
    if (!enableStack)
        return;

    textStack.remove(stackCurrentIndex + 1, textStack.count() - stackCurrentIndex - 1);
    textStack.push(item);
    ++stackCurrentIndex;
}

void RenameEdit::contextMenuEvent(QContextMenuEvent *e)
{
    // this menu only popup when preesed on viewport.
    // and pressing on the point that out of viewport but in RenameEdit will popoup other menu.
    e->accept();

    if (isReadOnly())
        return;

    QMenu *menu = createStandardContextMenu();
    if (!menu)
        return;

    // rewrite redo and undo, do not use them provided by QTextEdit.
    QAction *undoAction = menu->findChild<QAction *>(QStringLiteral("edit-undo"));
    QAction *redoAction = menu->findChild<QAction *>(QStringLiteral("edit-redo"));

    if (undoAction) {
        undoAction->setEnabled(stackCurrentIndex > 0);
        disconnect(undoAction, SIGNAL(triggered(bool)), nullptr, nullptr);
        connect(undoAction, &QAction::triggered, this, &RenameEdit::undo);
    }

    if (redoAction) {
        redoAction->setEnabled(stackCurrentIndex < textStack.count() - 1);
        QObject::disconnect(redoAction, SIGNAL(triggered(bool)), nullptr, nullptr);
        connect(redoAction, &QAction::triggered, this, &RenameEdit::redo);
    }

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void RenameEdit::focusOutEvent(QFocusEvent *e)
{
    // do not emit when focus out by showing menu.
    if (qApp->focusWidget() != this)
        QMetaObject::invokeMethod(parent(), "inputFocusOut", Qt::QueuedConnection);
    DTextEdit::focusOutEvent(e);
}

void RenameEdit::keyPressEvent(QKeyEvent *e)
{
    // rewrite redo and undo, do not use them provided by QTextEdit.
    if (e == QKeySequence::Undo) {
        undo();
        e->accept();
        return;
    } else if (e == QKeySequence::Redo) {
        redo();
        e->accept();
        return;
    }

    // key to commit
    // it can edit next or previous item if ingore Qt::Key_Tab or Qt::Key_Backtab.
    switch (e->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        e->accept();
        QMetaObject::invokeMethod(parent(), "inputFocusOut", Qt::QueuedConnection);
        return;
    default:
        break;
    }

    DTextEdit::keyPressEvent(e);
}
