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

#ifndef __KTE_SNIPPETS_REPOSITORY_H__
#define __KTE_SNIPPETS_REPOSITORY_H__

#include "kwidgetitemdelegate.h"
#include <QAbstractListModel>
#include <ktexteditor/codecompletionmodel.h>
#include <ktexteditor/document.h>
#include <ktexteditor_codesnippets_core_export.h>
#include <ktexteditor/templateinterface2.h>
#include <qdbusabstractadaptor.h>
#include <QDBusConnection>
class KConfigBase;
class KConfig;

namespace Ui {
  class KTESnippetRepository;
}

namespace KTextEditor {
  namespace CodesnippetsCore {
    class SnippetRepositoryEntry;
    class SnippetCompletionEntry;
    class SnippetCompletionModel;
    
    class SnippetRepositoryItemDelegatePrivate;
    
    class KTEXTEDITOR_CODESNIPPETS_CORE_EXPORT SnippetRepositoryItemDelegate: public KWidgetItemDelegate
    {
        Q_OBJECT
      public:
        explicit SnippetRepositoryItemDelegate(QAbstractItemView *itemView, QObject * parent = 0);
        virtual ~SnippetRepositoryItemDelegate();

        virtual QList<QWidget*> createItemWidgets() const;

        virtual void updateItemWidgets(const QList<QWidget*> widgets,
          const QStyleOptionViewItem &option,
          const QPersistentModelIndex &index) const;
          
        virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const;
        virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const;
      public Q_SLOTS:
        void enabledChanged(int state);
        void editEntry();
        void deleteEntry();      
      private:
        SnippetRepositoryItemDelegatePrivate* d;
    };
    

    class SnippetRepositoryModelPrivate;
    
    class KTEXTEDITOR_CODESNIPPETS_CORE_EXPORT SnippetRepositoryModel: public QAbstractListModel
    {
        Q_OBJECT
      public:
        explicit SnippetRepositoryModel(QObject *parent=0,KTextEditor::TemplateScriptRegistrar *scriptRegistrar=0);
        virtual ~SnippetRepositoryModel();
        enum Roles {
          NameRole = Qt::UserRole,
          FilenameRole,
          FiletypeRole,
          AuthorsRole,
          LicenseRole,
          SnippetLicenseRole,
          SystemFileRole,
          GhnsFileRole,
          EnabledRole,
          DeleteNowRole,
          EditNowRole,
          ForExtension=Qt::UserRole+100
        };
      private:
        void createOrUpdateList(bool update);
        void tokenNewHandled(const QString& token, const QString& filepath);
        QString m_dbusServiceName;
        QString m_dbusObjectPath;
        friend class SnippetRepositoryModelAdaptor;
        static long s_id;
        QDBusConnection m_connection;
      public:
        virtual int rowCount(const QModelIndex & parent = QModelIndex()) const;
        virtual QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
        virtual bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );
        void updateEntry(const QString& name, const QString& filename, const QString& filetype, const QString& authors, const QString& license, const QString& snippetlicense, bool systemFile, bool ghnsFile);
        void addEntry(const QString& name, const QString& filename, const QString& filetype, const QString& authors, const QString& license, const QString& snippetlicense, bool systemFile, bool ghnsFile, bool enabled);
        SnippetCompletionModel* completionModel(const QString &filetype);
        void readSessionConfig (KConfigBase* config, const QString& groupPrefix);
        void writeSessionConfig (KConfigBase* config, const QString& groupPrefix);

        QModelIndex indexForFile(const QString&);
        QModelIndex findFirstByName(const QString&);
      Q_SIGNALS:
        void typeChanged(const QStringList& fileType);
      public:
        void newEntry(QWidget *dialogParent,const QString& type=QString(),bool add_after_creation=false);
        void addSnippetToFile(QWidget *dialogParent, const QString& snippet, const QString& filename);
        void addSnippetToNewEntry(QWidget * dialogParent, const QString& snippet, const QString &repoTitle, const QString& type, bool add_after_creation);
      public Q_SLOTS:
        void newEntry();
        void copyToRepository(const KUrl& src);
      private:
        QList<SnippetRepositoryEntry> m_entries;
        void createOrUpdateListSub(KConfig& config,QStringList list, bool update, bool ghnsFile);
        class SnippetRepositoryModelPrivate *d;
        QStringList m_newTokens;
        TemplateScriptRegistrar *m_scriptRegistrar;
    };
    
    
    class SnippetRepositoryConfigWidgetPrivate;
    class KTEXTEDITOR_CODESNIPPETS_CORE_EXPORT SnippetRepositoryConfigWidget : public QWidget {
      Q_OBJECT
    public:
      explicit SnippetRepositoryConfigWidget( QWidget* parent, SnippetRepositoryModel *repository );
      virtual ~SnippetRepositoryConfigWidget();

    public Q_SLOTS:
      void slotCopy();
      void slotGHNS();
    private:
      SnippetRepositoryModel *m_repository;
      Ui::KTESnippetRepository *m_ui;
      SnippetRepositoryConfigWidgetPrivate *d;
  };

    

  }
}
#endif
// kate: space-indent on; indent-width 2; replace-tabs on;
