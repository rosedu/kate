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

#ifndef __JOWENN_SNIPPETS_SELECTOR_H__
#define __JOWENN_SNIPPETS_SELECTOR_H__

#include <kate/mainwindow.h>
#include <ktexteditor/view.h>
#include "ui_snippet_selector.h"
#include <qwidget.h>
#include <qpointer.h>
#include <QSortFilterProxyModel>

class QMenu;

namespace JoWenn {
  
  class KateSnippetsPlugin;

  class KateSnippetSelector: public QWidget, private Ui::KateSnippetSelector {
      Q_OBJECT
    public:
      KateSnippetSelector(Kate::MainWindow *mainWindow,JoWenn::KateSnippetsPlugin *plugin, QWidget *parent);
      virtual ~KateSnippetSelector();
    private Q_SLOTS:
      void viewChanged();
      void clicked(const QModelIndex& current);
      void doubleClicked(const QModelIndex& current);
      void typeChanged(KTextEditor::Document* document);
      void showHideSnippetText();
      void showRepoManager();
      void addSnippetToPopupAboutToShow();
      void addSnippetToClicked();
      void selectionChanged(KTextEditor::View *);
      void newRepo();
      void addSnippetToAction(QAction *action);
      void addSnippetToTriggered();
    Q_SIGNALS:
      void enableAdd(bool);
    private:
      JoWenn::KateSnippetsPlugin *m_plugin;
      Kate::MainWindow *m_mainWindow;
      QString m_mode;
      bool m_modelDrop;
      QMenu *m_addSnippetToPopup;
      QPointer<KTextEditor::View> m_associatedView;
      QPointer<KActionCollection> m_currentCollection;
    public:
      class ActionData {
        public:
          ActionData(){}
          ActionData(const QString &_filePath,const QString &_hlMode):filePath(_filePath),hlMode(_hlMode){}
          ~ActionData(){}
          QString filePath;
          QString hlMode;
      };
    
      QMenu *addSnippetToPopup(){return m_addSnippetToPopup;}
      
      void doPopupAddSnippetToPopup(const QString& fileType, const QString& data);
  
  private:
    QString m_modelDropData;
    QString m_modelDropFileType;
      
  };

  
  class KateSnippetSelectorProxyModel: public QSortFilterProxyModel {
    Q_OBJECT
  public:
    KateSnippetSelectorProxyModel(KateSnippetSelector *parent):QSortFilterProxyModel(parent),m_selector(parent) {
      setDynamicSortFilter(true);
    }
   virtual ~KateSnippetSelectorProxyModel(){}
    
    //DROPPING OF NEW SNIPPETS
        virtual Qt::DropActions supportedDropActions() const;        
        virtual Qt::ItemFlags flags(const QModelIndex &index) const;
        virtual QStringList mimeTypes() const;
        virtual bool dropMimeData(const QMimeData *data,
            Qt::DropAction action, int row, int column, const QModelIndex &parent);    
  private:
    KateSnippetSelector* m_selector;
  };
  
  
}

Q_DECLARE_METATYPE(JoWenn::KateSnippetSelector::ActionData)

#endif
// kate: space-indent on; indent-width 2; replace-tabs on;
