/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2001 Anders Lund <anders.lund@lund.tdcadsl.dk>
   Copyright (C) 2007 Mirko Stocker <me@misto.ch>
   Copyright (C) 2009 Dominik Haumann <dhaumann kde org>

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

#ifndef KATE_FILEBROWSER_CONFIG_H
#define KATE_FILEBROWSER_CONFIG_H

#include <KActionSelector>

#include <ktexteditor/configpage.h>
#include <kate/pluginconfigpageinterface.h>

#include "katefilebrowser.h"

class KateFileBrowserConfigPage : public Kate::PluginConfigPage
{
    Q_OBJECT

  public:
    explicit KateFileBrowserConfigPage( QWidget* parent = 0, const char *name = 0, KateFileBrowser *kfb = 0);
    virtual ~KateFileBrowserConfigPage()
    {}

    virtual void apply();
    virtual void reset();
    virtual void defaults()
    {}

  private Q_SLOTS:
    void slotMyChanged();

  private:
    void init();

    KateFileBrowser *fileBrowser;
    KActionSelector *acSel;

    bool m_changed;
};

#endif //KATE_FILEBROWSER_CONFIG_H

// kate: space-indent on; indent-width 2; replace-tabs on;
