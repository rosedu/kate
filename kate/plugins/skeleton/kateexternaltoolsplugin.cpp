/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2002 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2002 Anders Lund <anders.lund@lund.tdcadsl.dk>
 
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

#include "kateexternaltoolsplugin.h"
#include "kateexternaltoolsplugin.moc"

#include <kicon.h>
#include <kiconloader.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include <kparts/part.h>
#include <kaction.h>
#include <kactioncollection.h>

#include <kurl.h>
#include <klibloader.h>
#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>

#include <kpluginfactory.h>
#include <kauthorized.h>

K_PLUGIN_FACTORY(KateExternalToolsPluginFactory, registerPlugin<KateExternalToolsPlugin>();)
K_EXPORT_PLUGIN(KateExternalToolsPluginFactory("kateexternaltoolsplugin"))

KateExternalToolsPlugin::KateExternalToolsPlugin( QObject* parent, const QVariantList& ):
    Kate::Plugin ( (Kate::Application*)parent )
{}

Kate::PluginView *KateExternalToolsPlugin::createView (Kate::MainWindow *mainWindow)
{
  return new KateExternalToolsPluginView (mainWindow);
}

KateExternalToolsPluginView::KateExternalToolsPluginView (Kate::MainWindow *mainWindow)
    : Kate::PluginView (mainWindow), Kate::XMLGUIClient(KateExternalToolsPluginFactory::componentData())
{
  /*    actionCollection()->addAction( KStandardAction::Mail, this, SLOT(slotMail()) )
        ->setWhatsThis(i18n("Send one or more of the open documents as email attachments."));
      mainWindow->guiFactory()->addClient (this);*/
}

KateExternalToolsPluginView::~KateExternalToolsPluginView ()
{}
// kate: space-indent on; indent-width 2; replace-tabs on;

