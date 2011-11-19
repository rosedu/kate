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

//BEGIN Includes
#include "katefilebrowser.h"
#include "katefilebrowser.moc"

#include "katebookmarkhandler.h"

#include <ktexteditor/document.h>
#include <ktexteditor/view.h>

#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KDebug>
#include <KDirOperator>
#include <KFilePlacesModel>
#include <KHistoryComboBox>
#include <KLocale>
#include <KToolBar>
#include <KUrlNavigator>
#include <QAbstractItemView>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

#include <kdeversion.h>

//END Includes


KateFileBrowser::KateFileBrowser(Kate::MainWindow *mainWindow,
                                 QWidget * parent, const char * name)
  : KVBox (parent)
  , m_mainWindow(mainWindow)
{
  setObjectName(name);

  m_toolbar = new KToolBar(this);
  m_toolbar->setMovable(false);
  m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_toolbar->setContextMenuPolicy(Qt::NoContextMenu);

  // includes some actions, but not hooked into the shortcut dialog atm
  m_actionCollection = new KActionCollection(this);
  m_actionCollection->addAssociatedWidget(this);

  KFilePlacesModel* model = new KFilePlacesModel(this);
  m_urlNavigator = new KUrlNavigator(model, KUrl(QDir::homePath()), this);
  connect(m_urlNavigator, SIGNAL(urlChanged(KUrl)), SLOT(updateDirOperator(KUrl)));

  m_dirOperator = new KDirOperator(KUrl(), this);
  m_dirOperator->setView(KFile::/* Simple */Detail);
  m_dirOperator->view()->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_dirOperator->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));

#if KDE_IS_VERSION(4, 4, 60)
  // Mime filter for the KDirOperator
  QStringList filter;
  filter << "text/plain" << "text/html" << "inode/directory";
  m_dirOperator->setNewFileMenuSupportedMimeTypes(filter);
#endif

  setFocusProxy(m_dirOperator);
  connect(m_dirOperator, SIGNAL(viewChanged(QAbstractItemView*)),
          this, SLOT(selectorViewChanged(QAbstractItemView*)));
  connect(m_urlNavigator, SIGNAL(returnPressed()),
          m_dirOperator, SLOT(setFocus()));
  // now all actions exist in dir operator and we can use them in the toolbar
  setupActions();
  setupToolbar();

  KHBox* filterBox = new KHBox(this);

  QLabel* filterLabel = new QLabel(i18n("Filter:"), filterBox);
  m_filter = new KHistoryComboBox(true, filterBox);
  filterLabel->setBuddy(m_filter);
  m_filter->setMaxCount(10);
  m_filter->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));

  connect(m_filter, SIGNAL(editTextChanged(QString)),
          SLOT(slotFilterChange(QString)));
  connect(m_filter, SIGNAL(returnPressed(QString)),
          m_filter, SLOT(addToHistory(QString)));
  connect(m_filter, SIGNAL(returnPressed(QString)),
          m_dirOperator, SLOT(setFocus()));

  connect(m_dirOperator, SIGNAL(urlEntered(KUrl)),
          this, SLOT(updateUrlNavigator(KUrl)));

  // Connect the bookmark handler
  connect(m_bookmarkHandler, SIGNAL(openUrl(QString)),
          this, SLOT(setDir(QString)));

  m_filter->setWhatsThis(i18n("Enter a name filter to limit which files are displayed."));

  connect(m_dirOperator, SIGNAL(fileSelected(KFileItem)), this, SLOT(fileSelected(KFileItem)));
  connect(m_mainWindow, SIGNAL(viewChanged()), this, SLOT(autoSyncFolder()));
}

KateFileBrowser::~KateFileBrowser()
{
}
//END Constroctor/Destrctor

//BEGIN Public Methods
void KateFileBrowser::setupToolbar()
{
  KConfigGroup config(KGlobal::config(), "filebrowser");
  QStringList actions = config.readEntry( "toolbar actions", QStringList() );
  if ( actions.isEmpty() ) // default toolbar
    actions << "back" << "forward" << "bookmarks" << "sync_dir" << "configure";

  // remove all actions from the toolbar (there should be none)
  m_toolbar->clear();

  // now add all actions to the toolbar
  foreach (const QString& it, actions)
  {
    QAction *ac = 0;
    if (it.isEmpty()) continue;
    if (it == "bookmarks" || it == "sync_dir" || it == "configure")
      ac = actionCollection()->action(it);
    else
      ac = m_dirOperator->actionCollection()->action(it);

    if (ac)
      m_toolbar->addAction(ac);
  }
}

void KateFileBrowser::readSessionConfig(KConfigBase *config, const QString & name)
{
  KConfigGroup cgDir(config, name + ":dir");
  m_dirOperator->readConfig(cgDir);
  m_dirOperator->setView(KFile::Default);

  KConfigGroup cg(config, name);
  m_urlNavigator->setLocationUrl(cg.readPathEntry("location", QDir::homePath()));
  setDir(cg.readPathEntry("location", QDir::homePath()));
  m_autoSyncFolder->setChecked(cg.readEntry("auto sync folder", false));
  m_filter->setHistoryItems(cg.readEntry("filter history", QStringList()), true);
}

void KateFileBrowser::writeSessionConfig(KConfigBase *config, const QString & name)
{
  KConfigGroup cgDir(config, name + ":dir");
  m_dirOperator->writeConfig(cgDir);

  KConfigGroup cg = KConfigGroup(config, name);
  cg.writePathEntry("location", m_urlNavigator->locationUrl().url());
  cg.writeEntry("auto sync folder", m_autoSyncFolder->isChecked());
  cg.writeEntry("filter history", m_filter->historyItems());
}

//END Public Methods

//BEGIN Public Slots

void KateFileBrowser::slotFilterChange(const QString & nf)
{
  QString f = nf.trimmed();
  const bool empty = f.isEmpty() || f == "*";

  if (empty) {
    m_dirOperator->clearFilter();
  } else {
    m_dirOperator->setNameFilter(f);
  }

  m_dirOperator->updateDir();
}

bool kateFileSelectorIsReadable (const KUrl& url)
{
  if (!url.isLocalFile())
    return true; // what else can we say?

  QDir dir(url.toLocalFile());
  return dir.exists ();
}

void KateFileBrowser::setDir(KUrl u)
{
  KUrl newurl;

  if (!u.isValid())
    newurl.setPath(QDir::homePath());
  else
    newurl = u;

  QString pathstr = newurl.path(KUrl::AddTrailingSlash);
  newurl.setPath(pathstr);

  if (!kateFileSelectorIsReadable (newurl))
    newurl.cd(QString::fromLatin1(".."));

  if (!kateFileSelectorIsReadable (newurl))
    newurl.setPath(QDir::homePath());

  m_dirOperator->setUrl(newurl, true);
}

//END Public Slots

//BEGIN Private Slots

void KateFileBrowser::fileSelected(const KFileItem & /*file*/)
{
  openSelectedFiles();
}

void KateFileBrowser::openSelectedFiles()
{
  const KFileItemList list = m_dirOperator->selectedItems();

  foreach (const KFileItem& item, list)
  {
    m_mainWindow->openUrl(item.url());
  }

  m_dirOperator->view()->selectionModel()->clear();
}




void KateFileBrowser::updateDirOperator(const KUrl& u)
{
  m_dirOperator->setUrl(u, true);
}

void KateFileBrowser::updateUrlNavigator(const KUrl& u)
{
  m_urlNavigator->setLocationUrl(u);
}

void KateFileBrowser::setActiveDocumentDir()
{
//   kDebug(13001)<<"KateFileBrowser::setActiveDocumentDir()";
  KUrl u = activeDocumentUrl();
//   kDebug(13001)<<"URL: "<<u.prettyUrl();
  if (!u.isEmpty())
    setDir(u.upUrl());
//   kDebug(13001)<<"... setActiveDocumentDir() DONE!";
}

void KateFileBrowser::autoSyncFolder()
{
  if (m_autoSyncFolder->isChecked()) {
    setActiveDocumentDir();
  }
}


void KateFileBrowser::selectorViewChanged(QAbstractItemView * newView)
{
  newView->setSelectionMode(QAbstractItemView::ExtendedSelection);
}

//END Private Slots

//BEGIN Protected

KUrl KateFileBrowser::activeDocumentUrl()
{
  KTextEditor::View *v = m_mainWindow->activeView();
  if (v)
    return v->document()->url();
  return KUrl();
}

void KateFileBrowser::setupActions()
{
  // bookmarks action!
  KActionMenu *acmBookmarks = new KActionMenu(KIcon("bookmarks"), i18n("Bookmarks"), this);
  acmBookmarks->setDelayed(false);
  m_bookmarkHandler = new KateBookmarkHandler(this, acmBookmarks->menu());
  acmBookmarks->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // action for synchronizing the dir operator with the current document path
  KAction* syncFolder = new KAction(this);
  syncFolder->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  syncFolder->setText(i18n("Current Document Folder"));
  syncFolder->setIcon(KIcon("system-switch-user"));
  connect(syncFolder, SIGNAL(triggered()), this, SLOT(setActiveDocumentDir()));

  m_actionCollection->addAction("sync_dir", syncFolder);
  m_actionCollection->addAction("bookmarks", acmBookmarks);

  // section for settings menu
  KActionMenu *optionsMenu = new KActionMenu(KIcon("configure"), i18n("Options"), this);
  optionsMenu->setDelayed(false);
  optionsMenu->addAction(m_dirOperator->actionCollection()->action("short view"));
  optionsMenu->addAction(m_dirOperator->actionCollection()->action("detailed view"));
  optionsMenu->addAction(m_dirOperator->actionCollection()->action("tree view"));
  optionsMenu->addAction(m_dirOperator->actionCollection()->action("detailed tree view"));
  optionsMenu->addSeparator();
  optionsMenu->addAction(m_dirOperator->actionCollection()->action("show hidden"));

  // action for synchronising the dir operator with the current document path
  m_autoSyncFolder = new KAction(this);
  m_autoSyncFolder->setCheckable(true);
  m_autoSyncFolder->setText(i18n("Automatically synchronize with current document"));
  m_autoSyncFolder->setIcon(KIcon("system-switch-user"));
  connect(m_autoSyncFolder, SIGNAL(triggered()), this, SLOT(autoSyncFolder()));
  optionsMenu->addAction(m_autoSyncFolder);

  m_actionCollection->addAction("configure", optionsMenu);
  
  //
  // Remove all shortcuts due to shortcut clashes (e.g. F5: reload, Ctrl+B: bookmark)
  // BUGS: #188954, #236368
  //
  foreach (QAction* a, m_actionCollection->actions()) {
    a->setShortcut(QKeySequence());
  }
  foreach (QAction* a, m_dirOperator->actionCollection()->actions()) {
    a->setShortcut(QKeySequence());
  }
}
//END Protected

// kate: space-indent on; indent-width 2; replace-tabs on;
