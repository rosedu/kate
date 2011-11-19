/* This file is part of the KDE project
 * Copyright (C) 2009 Joseph Wenninger <jowenn@kde.org>
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include <ktexteditor_codesnippets_core_export.h>
#include<QObject>
#include<QDBusAbstractAdaptor>

#ifndef DBUS_HELPERS_H
#define DBUS_HELPERS_H

namespace KTextEditor {
  namespace CodesnippetsCore {
#ifndef SNIPPET_EDITOR
    class SnippetRepositoryModel;
    class SnippetRepositoryModelAdaptor: public QDBusAbstractAdaptor
    {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.Kate.Plugin.SnippetsTNG.Repository")
      public:
        SnippetRepositoryModelAdaptor(SnippetRepositoryModel *repository);
        virtual ~SnippetRepositoryModelAdaptor();
      public Q_SLOTS:
        void updateSnippetRepository();
        void tokenNewHandled(const QString& token, const QString& filepath);
      private:
        SnippetRepositoryModel* m_repository;
    };
#endif

#ifdef SNIPPET_EDITOR
  namespace Editor {
    void  notifyTokenNewHandled(const QString& token, const QString& service, const QString& object, const QString&filePath);
#endif
    KTEXTEDITOR_CODESNIPPETS_CORE_EXPORT void notifyRepos(); 
   
#ifdef SNIPPET_EDITOR
  }
#endif
  }
}

#endif
