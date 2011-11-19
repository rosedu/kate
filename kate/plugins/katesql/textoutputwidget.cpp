/*
   Copyright (C) 2010  Marco Mentasti  <marcomentasti@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "textoutputwidget.h"
#include "connection.h"

#include <kglobalsettings.h>
#include <kdebug.h>
#include <ktoolbar.h>
#include <kaction.h>
#include <klocale.h>

#include <qlayout.h>
#include <qtextedit.h>
#include <qdatetime.h>
#include <qsqlquery.h>
#include <qsqldriver.h>
#include <qsqlerror.h>

TextOutputWidget::TextOutputWidget(QWidget *parent)
: QWidget(parent)
{
  m_succesTextColor = QColor::fromRgb(3, 191, 3);
  m_succesBackgroundColor = QColor::fromRgb(231, 247, 231);

  m_errorTextColor = QColor::fromRgb(191, 3, 3);
  m_errorBackgroundColor = QColor::fromRgb(247, 231, 231);

  m_layout = new QHBoxLayout(this);

  m_output = new QTextEdit();
  m_output->setReadOnly(true);

  QFont fixedFont(KGlobalSettings::fixedFont());

  m_output->setCurrentFont(fixedFont);

  KToolBar *toolbar = new KToolBar(this);
  toolbar->setOrientation(Qt::Vertical);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  toolbar->setIconSize(QSize(16, 16));

  /// TODO: disable actions if no results are displayed

  KAction *action;

  action = new KAction( KIcon("edit-clear"), i18nc("@action:intoolbar", "Clear"), this);
  toolbar->addAction(action);
  connect(action, SIGNAL(triggered()), m_output, SLOT(clear()));

  m_layout->addWidget(toolbar);
  m_layout->addWidget(m_output, 1);

  setLayout(m_layout);
}

TextOutputWidget::~TextOutputWidget()
{
}


void TextOutputWidget::showErrorMessage(const QString &message)
{
  QColor previousBackgroundColor = m_output->textBackgroundColor();
  QColor previousColor = m_output->textColor();

  m_output->setTextBackgroundColor(m_errorBackgroundColor);
  m_output->setTextColor(m_errorTextColor);

  writeMessage(message);

  m_output->setTextBackgroundColor(previousBackgroundColor);
  m_output->setTextColor(previousColor);
}


void TextOutputWidget::showSuccessMessage(const QString &message)
{
  QColor previousBackgroundColor = m_output->textBackgroundColor();
  QColor previousColor = m_output->textColor();

  m_output->setTextBackgroundColor(m_succesBackgroundColor);
  m_output->setTextColor(m_succesTextColor);

  writeMessage(message);

  m_output->setTextBackgroundColor(previousBackgroundColor);
  m_output->setTextColor(previousColor);
}

void TextOutputWidget::writeMessage(const QString& msg)
{
  m_output->append(QString("%1: %2\n").arg(QDateTime::currentDateTime().toString(Qt::SystemLocaleDate)).arg(msg));

  raise();
}
