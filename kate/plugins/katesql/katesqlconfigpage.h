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

#ifndef KATESQLCONFIGPAGE_H
#define KATESQLCONFIGPAGE_H

class OutputStyleWidget;
class QCheckBox;

#include "katesqlplugin.h"

#include <kate/pluginconfigpageinterface.h>

/// TODO: add options to change datetime and numbers format

class KateSQLConfigPage : public Kate::PluginConfigPage
{
  Q_OBJECT

  public:
    explicit KateSQLConfigPage( QWidget* parent = 0 );
    virtual ~KateSQLConfigPage();

  public slots:
    virtual void apply();
    virtual void reset();
    virtual void defaults();

  private:
    KateSQLPlugin *m_plugin;
    QCheckBox *m_box;
    OutputStyleWidget *m_outputStyleWidget;

  signals:
    void settingsChanged();
};

#endif // KATESQLCONFIGPAGE_H

