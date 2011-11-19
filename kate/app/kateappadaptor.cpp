/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
 
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

#include "kateappadaptor.h"

#include "kateapp.h"
#include "katesession.h"
#include "katedocmanager.h"
#include "katemainwindow.h"
#include "kateappadaptor.moc"

#include <kdebug.h>
#include <kwindowsystem.h>

KateAppAdaptor::KateAppAdaptor (KateApp *app)
    : QDBusAbstractAdaptor( app )
    , m_app (app)
{}

void KateAppAdaptor::activate ()
{
  KateMainWindow *win = m_app->activeMainWindow();
  if (!win)
    return;

  win->show();
  win->activateWindow();
  win->raise();
  
#ifdef Q_WS_X11
  KWindowSystem::forceActiveWindow (win->winId ());
  KWindowSystem::raiseWindow (win->winId ());
#endif
}

QDBusObjectPath KateAppAdaptor::documentManager ()
{
  return QDBusObjectPath (m_app->documentManager()->dbusObjectPath ());
}

QDBusObjectPath KateAppAdaptor::activeMainWindow ()
{
  KateMainWindow *win = m_app->activeMainWindow();

  if (win)
    return QDBusObjectPath (win->dbusObjectPath());

  return QDBusObjectPath ();
}

uint KateAppAdaptor::activeMainWindowNumber ()
{
  KateMainWindow *win = m_app->activeMainWindow();

  if (win)
    return win->mainWindowNumber ();

  return 0;
}


uint KateAppAdaptor::mainWindows ()
{
  return m_app->mainWindows ();
}

QDBusObjectPath KateAppAdaptor::mainWindow (uint n)
{
  KateMainWindow *win = m_app->mainWindow(n);

  if (win)
    return QDBusObjectPath (win->dbusObjectPath ());

  return QDBusObjectPath ();
}

bool KateAppAdaptor::openUrl (QString url, QString encoding)
{
  return m_app->openUrl (url, encoding, false);
}

bool KateAppAdaptor::openUrl (QString url, QString encoding, bool isTempFile)
{
  kDebug () << "openURL";

  return m_app->openUrl (url, encoding, isTempFile);
}

//-----------
QString KateAppAdaptor::tokenOpenUrl (QString url, QString encoding)
{
  KTextEditor::Document *doc=m_app->openDocUrl (url, encoding, false);
  if (!doc) return QString("ERROR");
  return QString("%1").arg((qptrdiff)doc);
}

QString KateAppAdaptor::tokenOpenUrl (QString url, QString encoding, bool isTempFile)
{
  kDebug () << "openURL";
  KTextEditor::Document *doc=m_app->openDocUrl (url, encoding, isTempFile);
  if (!doc) return QString("ERROR");
  return QString("%1").arg((qptrdiff)doc);
}
//--------

bool KateAppAdaptor::setCursor (int line, int column)
{
  return m_app->setCursor (line, column);
}

bool KateAppAdaptor::openInput (QString text)
{
  return m_app->openInput (text);
}

bool KateAppAdaptor::activateSession (QString session)
{
  m_app->sessionManager()->activateSession (m_app->sessionManager()->giveSession (session));

  return true;
}

QString KateAppAdaptor::activeSession()
{
  return m_app->sessionManager()->activeSession()->sessionName();
}

void KateAppAdaptor::emitExiting ()
{
  emit exiting (); 
}

void KateAppAdaptor::emitDocumentClosed(const QString& token)
{
  documentClosed(token);
}

// kate: space-indent on; indent-width 2; replace-tabs on;
