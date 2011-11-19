/* This file is part of the KDE project
   Copyright (C) 2011 Dominik Haumann <dhaumann kde org>

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

#ifndef _katemainwindow_adaptor_h_
#define _katemainwindow_adaptor_h_

#include <QtDBus/QtDBus>

class KateMainWindow;

class KateMainWindowAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.Kate.MainWindow")

  public:
    KateMainWindowAdaptor (KateMainWindow *w);

  public Q_SLOTS:
    QString activeDocumentUrl() const;
    uint mainWindowNumber() const;

  private:
    KateMainWindow *m_w;
};
#endif
// kate: space-indent on; indent-width 2; replace-tabs on;

