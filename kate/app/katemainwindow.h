/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2001 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2001 Anders Lund <anders.lund@lund.tdcadsl.dk>

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

#ifndef __KATE_MAINWINDOW_H__
#define __KATE_MAINWINDOW_H__

#include "katemain.h"
#include "katemdi.h"

#include <KTextEditor/View>
#include <KTextEditor/Document>

#include <KParts/Part>

#include <KAction>

#include <QDragEnterEvent>
#include <QEvent>
#include <QDropEvent>
#include <QVBoxLayout>
#include <QModelIndex>
#include <QHash>

class QMenu;

namespace KIO
{
  class UDSEntry;
  typedef class QList<UDSEntry> UDSEntryList;
}

namespace Kate
{
  class MainWindow;
  class Plugin;
  class PluginView;
  class PluginConfigPageInterface;
}

class KFileItem;
class KRecentFilesAction;

class KateViewManager;
class KateMwModOnHdDialog;

#include <QtGui/QStackedLayout>
// Helper layout class to always provide minimum size
class KateContainerStackedLayout : public QStackedLayout
{
  Q_OBJECT
public:
  KateContainerStackedLayout(QWidget* parent);
  virtual QSize sizeHint() const;
  virtual QSize minimumSize() const;
};


class KateMainWindow : public KateMDI::MainWindow, virtual public KParts::PartBase
{
    Q_OBJECT

    friend class KateConfigDialog;
    friend class KateViewManager;

  public:
    /**
     * Construct the window and restore it's state from given config if any
     * @param sconfig session config for this window, 0 if none
     * @param sgroup session config group to use
     */
    KateMainWindow (KConfig *sconfig, const QString &sgroup);

    /**
     * Destruct the nice window
     */
    ~KateMainWindow();

    /**
     * Accessor methodes for interface and child objects
     */
  public:
    Kate::MainWindow *mainWindow ()
    {
      return m_mainWindow;
    }

    KateViewManager *viewManager ()
    {
      return m_viewManager;
    }

    QString dbusObjectPath() const
    {
      return m_dbusObjectPath;
    }
    /**
     * various methodes to get some little info out of this
     */
  public:
    /** Returns the URL of the current document.
     * anders: I add this for use from the file selector. */
    KUrl activeDocumentUrl();

    uint mainWindowNumber () const
    {
      return myID;
    }

    /**
     * Prompts the user for what to do with files that are modified on disk if any.
     * This is optionally run when the window receives focus, and when the last
     * window is closed.
     * @return true if no documents are modified on disk, or all documents were
     * handled by the dialog; otherwise (the dialog was canceled) false.
     */
    bool showModOnDiskPrompt();

  public:
    /*reimp*/ void readProperties(const KConfigGroup& config);
    /*reimp*/ void saveProperties(KConfigGroup& config);
    /*reimp*/ void saveGlobalProperties( KConfig* sessionConfig );

  public:
    bool queryClose_internal(KTextEditor::Document *doc = NULL);

    /**
     * save the settings, size and state of this window in
     * the provided config group
     */
    void saveWindowConfig(const KConfigGroup &);
    /**
     * restore the settings, size and state of this window from
     * the provided config group.
     */
    void restoreWindowConfig(const KConfigGroup &);

  private:
    /**
     * Setup actions which pointers are needed already in setupMainWindow
     */
    void setupImportantActions ();

    void setupMainWindow();
    void setupActions();
    bool queryClose();

    /**
     * read some global options from katerc
     */
    void readOptions();

    /**
     * save some global options to katerc
     */
    void saveOptions();

    void dragEnterEvent( QDragEnterEvent * );
    void dropEvent( QDropEvent * );

  public Q_SLOTS:
    void slotFileClose();
    void slotFileQuit();
    void queueModifiedOnDisc(KTextEditor::Document *doc);

    /**
     * slots used for actions in the menus/toolbars
     * or internal signal connections
     */
  private Q_SLOTS:
    void newWindow ();

    void slotConfigure();

    void slotOpenWithMenuAction(QAction* a);

    void slotEditToolbars();
    void slotNewToolbarConfig();
    void slotWindowActivated ();
    void slotUpdateOpenWith();
    void slotOpenDocument(KUrl);

    void slotDropEvent(QDropEvent *);
    void editKeys();
    void mSlotFixOpenWithMenu();

    void tipOfTheDay();

    /* to update the caption */
    void slotDocumentCreated (KTextEditor::Document *doc);
    void updateCaption (KTextEditor::Document *doc);
    // calls updateCaption(doc) with the current document
    void updateCaption ();

    void pluginHelp ();
    void aboutEditor();
    void slotFullScreen(bool);

    void slotListRecursiveEntries(KIO::Job *job, const KIO::UDSEntryList &list);

  private Q_SLOTS:
    void toggleShowStatusBar ();

  public:
    bool showStatusBar ();

  Q_SIGNALS:
    void statusBarToggled ();

  public:
    void openUrl (const QString &name = QString());

    QHash<Kate::Plugin*, Kate::PluginView*> &pluginViews ()
    {
      return m_pluginViews;
    }

    inline QWidget *bottomViewBarContainer() {return m_bottomViewBarContainer;}
    inline void addToBottomViewBarContainer(KTextEditor::View *view,QWidget *bar){m_bottomContainerStack->addWidget (bar); m_bottomViewBarMapping[view]=BarState(bar);}
    inline void hideBottomViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_bottomViewBarMapping.value(view); bar=state.bar(); if (bar) {m_bottomContainerStack->setCurrentWidget(bar); bar->hide(); state.setState(false); m_bottomViewBarMapping[view]=state;} m_bottomViewBarContainer->hide();}
    inline void showBottomViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_bottomViewBarMapping.value(view); bar=state.bar();  if (bar) {m_bottomContainerStack->setCurrentWidget(bar); bar->show(); state.setState(true); m_bottomViewBarMapping[view]=state;  m_bottomViewBarContainer->show();}}
    inline void deleteBottomViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_bottomViewBarMapping.take(view); bar=state.bar();  if (bar) {if (m_bottomContainerStack->currentWidget()==bar) m_bottomViewBarContainer->hide(); delete bar;}}

    inline QWidget *topViewBarContainer() {return m_topViewBarContainer;}
    inline void addToTopViewBarContainer(KTextEditor::View *view,QWidget *bar){m_topContainerStack->addWidget (bar); m_topViewBarMapping[view]=BarState(bar);}
    inline void hideTopViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_topViewBarMapping.value(view); bar=state.bar(); if (bar) {m_topContainerStack->setCurrentWidget(bar); bar->hide(); state.setState(false); m_topViewBarMapping[view]=state;} m_topViewBarContainer->hide();}
    inline void showTopViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_topViewBarMapping.value(view); bar=state.bar();  if (bar) {m_topContainerStack->setCurrentWidget(bar); bar->show(); state.setState(true); m_topViewBarMapping[view]=state;  m_topViewBarContainer->show();}}
    inline void deleteTopViewBarForView(KTextEditor::View *view) {QWidget *bar; BarState state=m_topViewBarMapping.take(view); bar=state.bar();  if (bar) {if (m_topContainerStack->currentWidget()==bar) m_topViewBarContainer->hide(); delete bar;}}

  private Q_SLOTS:
    void slotUpdateBottomViewBar();
    void slotUpdateTopViewBar();
    
  private Q_SLOTS:
    void slotDocumentCloseAll();
    void slotDocumentCloseOther();
    void slotDocumentCloseOther(KTextEditor::Document *document);
    void slotDocumentCloseSelected(const QList<KTextEditor::Document*>&);
  private:
    static uint uniqueID;
    uint myID;

    Kate::MainWindow *m_mainWindow;

    bool modNotification;

    // management items
    KateViewManager *m_viewManager;

    KRecentFilesAction *fileOpenRecent;

    KActionMenu* documentOpenWith;

    KToggleAction* settingsShowFileselector;

    bool m_modignore, m_grrr;

    QString m_dbusObjectPath;

    // all plugin views for this mainwindow, used by the pluginmanager
    QHash<Kate::Plugin*, Kate::PluginView*> m_pluginViews;

    // options: show statusbar + show path
    KToggleAction *m_paShowPath;
    KToggleAction *m_paShowStatusBar;
    QWidget *m_bottomViewBarContainer;
    KateContainerStackedLayout *m_bottomContainerStack;
    QWidget *m_topViewBarContainer;
    KateContainerStackedLayout *m_topContainerStack;

    class BarState{
      public:
        BarState():m_bar(0),m_state(false){}
        BarState(QWidget* bar):m_bar(bar),m_state(false){}
        ~BarState(){}
        QWidget *bar(){return m_bar;}
        bool state(){return m_state;}
        void setState(bool state){m_state=state;}
      private:
        QWidget *m_bar;
        bool m_state;
    };
    QHash<KTextEditor::View*,BarState> m_bottomViewBarMapping;
    QHash<KTextEditor::View*,BarState> m_topViewBarMapping;

  public:
    static void unsetModifiedOnDiscDialogIfIf(KateMwModOnHdDialog* diag) {
      if (s_modOnHdDialog==diag) s_modOnHdDialog=0;
    }
  private:
    static KateMwModOnHdDialog *s_modOnHdDialog;

  public Q_SLOTS:
    void showPluginConfigPage(Kate::PluginConfigPageInterface *configpageinterface,uint id);  

};

#endif

// kate: space-indent on; indent-width 2; replace-tabs on;
