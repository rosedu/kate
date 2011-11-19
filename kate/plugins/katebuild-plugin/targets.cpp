//
// Description: Widget for configuring build targets
//
// Copyright (c) 2011 Kåre Särs <kare.sars@iki.fi>
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License version 2 as published by the Free Software Foundation.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public License
//  along with this library; see the file COPYING.LIB.  If not, write to
//  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//  Boston, MA 02110-1301, USA.

#include "targets.h"
#include "targets.moc"
#include <klocale.h>
#include <kdebug.h>
#include <QApplication>

TargetsUi::TargetsUi(QWidget *parent):
QWidget(parent)
{
    targetCombo = new KComboBox(this);
    targetCombo->setEditable(true);
    targetCombo->setInsertPolicy(QComboBox::InsertAtCurrent);
    connect(targetCombo, SIGNAL(editTextChanged(QString)), this, SLOT(editTarget(QString)));
    
    newTarget = new QToolButton(this);
    newTarget->setToolTip(i18n("New"));
    newTarget->setIcon(KIcon("document-new"));
    
    copyTarget = new QToolButton(this);
    copyTarget->setToolTip(i18n("Copy"));
    copyTarget->setIcon(KIcon("edit-copy"));

    deleteTarget = new QToolButton(this);
    deleteTarget->setToolTip(i18n("Delete"));
    deleteTarget->setIcon(KIcon("edit-delete"));
    
    line = new QFrame(this);
    line->setFrameShadow(QFrame::Sunken);
    
    dirLabel = new QLabel(i18n("Working directory"), this);
    buildDir = new KLineEdit(this);
    buildDir->setToolTip(i18n("Leave empty to use the directory of the current document. "));
    buildDir->setClearButtonShown(true);
    browse = new QToolButton(this);
    browse->setIcon(KIcon("inode-directory"));
    
    buildLabel = new QLabel(i18n("Build"), this);
    buildCmd = new KLineEdit(this);
    buildCmd->setClearButtonShown(true);
    
    cleanLabel = new QLabel(i18n("Clean"), this);
    cleanCmd = new KLineEdit(this);
    cleanCmd->setClearButtonShown(true);
    
    quickLabel = new QLabel(i18n("Quick compile"), this);
    quickCmd = new KLineEdit(this);
    quickCmd->setToolTip(i18n("Use:\n\"%f\" for current file\n\"%d\" for directory of current file"));
    quickCmd->setClearButtonShown(true);
    
    dirLabel->setBuddy(buildDir);
    buildLabel->setBuddy(buildCmd);
    cleanLabel->setBuddy(cleanCmd);
    quickLabel->setBuddy(quickCmd);
    
    // calculate the approximate height to exceed before going to "Side Layout"
    setSideLayout();
    m_widgetsHeight = sizeHint().height();
    delete layout();
    
    setBottomLayout();
    m_useBottomLayout = true;
}

void TargetsUi::resizeEvent(QResizeEvent *)
{
    // check if the widgets fit in a VBox layout
    if (m_useBottomLayout && (size().height() > m_widgetsHeight) && (size().width() < m_widgetsHeight * 2.5))
    {
        delete layout();
        setSideLayout();
        m_useBottomLayout = false;
    }
    else if (!m_useBottomLayout && ((size().height() < m_widgetsHeight) || (size().width() > m_widgetsHeight * 2.5)))
    {
        delete layout();
        setBottomLayout();
        m_useBottomLayout = true;
    }
}

void TargetsUi::setSideLayout()
{
    QGridLayout* layout = new QGridLayout(this);
    QHBoxLayout* tLayout = new QHBoxLayout();
    tLayout->addWidget(targetCombo, 1);
    tLayout->addWidget(newTarget, 0);
    tLayout->addWidget(copyTarget, 0);
    tLayout->addWidget(deleteTarget, 0);
    tLayout->setContentsMargins(0,0,0,0);

    layout->addLayout(tLayout, 0, 0, 1, 4);

    line->setFrameShape(QFrame::HLine);
    layout->addWidget(line, 1, 0, 1, 4);
    
    layout->addWidget(dirLabel, 2, 0, Qt::AlignLeft);
    layout->addWidget(buildDir, 3, 0, 1, 3);
    layout->addWidget(browse, 3, 3);
    
    layout->addWidget(buildLabel, 4, 0, Qt::AlignLeft);
    layout->addWidget(buildCmd, 5, 0, 1, 4);
    
    layout->addWidget(cleanLabel, 6, 0, Qt::AlignLeft);
    layout->addWidget(cleanCmd, 7, 0, 1, 4);
    
    layout->addWidget(quickLabel, 8, 0, Qt::AlignLeft);
    layout->addWidget(quickCmd, 9, 0, 1, 4);
    
    layout->addItem(new QSpacerItem(1, 1), 10, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(12, 1);
}

void TargetsUi::setBottomLayout()
{
    QGridLayout* layout = new QGridLayout(this);
    layout->addWidget(targetCombo, 0, 0);
    
    QHBoxLayout* tLayout = new QHBoxLayout();
    tLayout->addWidget(newTarget, 0);
    tLayout->addWidget(copyTarget, 0);
    tLayout->addWidget(deleteTarget, 0);
    tLayout->setContentsMargins(0,0,0,0);
    
    layout->addLayout(tLayout, 1, 0);
    
    line->setFrameShape(QFrame::VLine);
    layout->addWidget(line, 0, 1, 5, 1);

    layout->addWidget(dirLabel, 0, 2, Qt::AlignRight);
    layout->addWidget(buildDir, 0, 3, 1, 2);
    layout->addWidget(browse, 0, 5);
    
    layout->addWidget(buildLabel, 1, 2, Qt::AlignRight);
    layout->addWidget(buildCmd, 1, 3, 1, 3);
    
    layout->addWidget(cleanLabel, 2, 2, Qt::AlignRight);
    layout->addWidget(cleanCmd, 2, 3, 1, 3);
    
    layout->addWidget(quickLabel, 3, 2, Qt::AlignRight);
    layout->addWidget(quickCmd, 3, 3, 1, 3);
    
    layout->addItem(new QSpacerItem(1, 1), 4, 0 );
    layout->setColumnStretch(3, 1);
    layout->setRowStretch(5, 1);
}

void TargetsUi::editTarget(const QString &text)
{
    int curPos = targetCombo->lineEdit()->cursorPosition();
    targetCombo->setItemText(targetCombo->currentIndex(), text);
    targetCombo->lineEdit()->setCursorPosition(curPos);
}

