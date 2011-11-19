/***************************************************************************
 *                        plugin_katesymbolviewer.cpp  -  description
 *                           -------------------
 *  begin                : Apr 2 2003
 *  author               : 2003 Massimo Callegari
 *  email                : massimocallegari@yahoo.it
 *
 *  Changes:
 *  Nov 09 2004 v.1.3 - For changelog please refer to KDE CVS
 *  Nov 05 2004 v.1.2 - Choose parser from the current highlight. Minor i18n changes.
 *  Nov 28 2003 v.1.1 - Structured for multilanguage support
 *                      Added preliminary Tcl/Tk parser (thanks Rohit). To be improved.
 *                      Various bugfixing.
 *  Jun 19 2003 v.1.0 - Removed QTimer (polling is Evil(tm)... )
 *                      - Captured documentChanged() event to refresh symbol list
 *                      - Tooltips vanished into nowhere...sigh :(
 *  May 04 2003 v 0.6 - Symbol List becomes a K3ListView object. Removed Tooltip class.
 *                      Added a QTimer that every 200ms checks:
 *                      * if the list width has changed
 *                      * if the document has changed
 *                      Added an entry in the popup menu to switch between List and Tree mode
 *                      Various bugfixing.
 *  Apr 24 2003 v 0.5 - Added three check buttons in popup menu to show/hide symbols
 *  Apr 23 2003 v 0.4 - "View Symbol" moved in Settings menu. "Refresh List" is no
 *                      longer in Kate menu. Moved into a popup menu activated by a
 *                      mouse right button click. + Bugfixing.
 *  Apr 22 2003 v 0.3 - Added macro extraction + several bugfixing
 *  Apr 19 2003 v 0.2 - Added to CVS. Extract functions and structures
 *  Apr 07 2003 v 0.1 - First version.
 *
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "plugin_katesymbolviewer.h"
#include "plugin_katesymbolviewer.moc"

#include <kaction.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kaboutdata.h>
#include <kfiledialog.h>
#include <ktoggleaction.h>
#include <kactioncollection.h>
#include <kiconloader.h>

#include <QPixmap>
#include <QVBoxLayout>
#include <QGroupBox>

#include <QResizeEvent>
#include <QMenu>

K_PLUGIN_FACTORY(KateSymbolViewerFactory, registerPlugin<KatePluginSymbolViewer>();)
K_EXPORT_PLUGIN(KateSymbolViewerFactory(KAboutData("katesymbolviewer","katesymbolviewer",ki18n("SymbolViewer"), "0.1", ki18n("View symbols"), KAboutData::License_LGPL_V2)) )


KatePluginSymbolViewerView2::KatePluginSymbolViewerView2 (QList<KatePluginSymbolViewerView *> &views, Kate::MainWindow *w)
    : Kate::PluginView(w)
    , m_views (views)
{
    m_view = new KatePluginSymbolViewerView(w);
    m_views.append (m_view);
}

KatePluginSymbolViewerView2::~KatePluginSymbolViewerView2 ()
{
    m_views.removeAll(m_view);
    delete m_view;
}

KatePluginSymbolViewerView* KatePluginSymbolViewerView2::view()
{
    return m_view;
}

KatePluginSymbolViewerView::KatePluginSymbolViewerView(Kate::MainWindow *w) : Kate::XMLGUIClient(KateSymbolViewerFactory::componentData())
{
  KGlobal::locale()->insertCatalog("katesymbolviewer");
  KToggleAction *act = actionCollection()->add<KToggleAction>("view_insert_symbolviewer");
  act->setText(i18n("Hide Symbols"));
  connect(act,SIGNAL(toggled(bool)),this,SLOT(slotInsertSymbol()));
  act->setCheckedState(KGuiItem(i18n("Show Symbols")));

  w->guiFactory()->addClient (this);
  win = w;
  symbols = 0;

  m_Active = false;
  popup = new QMenu(symbols);
  popup->insertItem(i18n("Refresh List"), this, SLOT(slotRefreshSymbol()));
  popup->addSeparator();
  m_macro = popup->insertItem(i18n("Show Macros"), this, SLOT(toggleShowMacros()));
  m_struct = popup->insertItem(i18n("Show Structures"), this, SLOT(toggleShowStructures()));
  m_func = popup->insertItem(i18n("Show Functions"), this, SLOT(toggleShowFunctions()));
  popup->addSeparator();
  popup->insertItem(i18n("List/Tree Mode"), this, SLOT(slotChangeMode()));
  m_sort = popup->insertItem(i18n("Enable Sorting"), this, SLOT(slotEnableSorting()));

  popup->setItemChecked(m_macro, true);
  popup->setItemChecked(m_struct, true);
  popup->setItemChecked(m_func, true);
  popup->setItemChecked(m_sort, false);
  macro_on = true;
  struct_on = true;
  func_on = true;
  treeMode = false;
  lsorting = false;
  types_on = KConfigGroup(KGlobal::config(), "PluginSymbolViewer").readEntry("ViewTypes", false);
  expanded_on = KConfigGroup(KGlobal::config(), "PluginSymbolViewer").readEntry("ExpandTree", false);
  slotInsertSymbol();
}

KatePluginSymbolViewerView::~KatePluginSymbolViewerView()
{
  win->guiFactory()->removeClient (this);
  delete dock;
  delete popup;
}

void KatePluginSymbolViewerView::toggleShowMacros(void)
{
 bool s = !popup->isItemChecked(m_macro);
 popup->setItemChecked(m_macro, s);
 macro_on = s;
 slotRefreshSymbol();
}

void KatePluginSymbolViewerView::toggleShowStructures(void)
{
 bool s = !popup->isItemChecked(m_struct);
 popup->setItemChecked(m_struct, s);
 struct_on = s;
 slotRefreshSymbol();
}

void KatePluginSymbolViewerView::toggleShowFunctions(void)
{
 bool s = !popup->isItemChecked(m_func);
 popup->setItemChecked(m_func, s);
 func_on = s;
 slotRefreshSymbol();
}

void KatePluginSymbolViewerView::slotInsertSymbol()
{
 QPixmap cls( ( const char** ) class_xpm );
 QStringList titles;

 if (m_Active == false)
     {
      dock = win->createToolView("kate_plugin_symbolviewer", Kate::MainWindow::Left, cls, i18n("Symbol List"));

      symbols = new QTreeWidget(dock);
      symbols->setLayoutDirection( Qt::LeftToRight );
      treeMode = 0;

      connect(symbols, SIGNAL(itemActivated(QTreeWidgetItem*,int)), this, SLOT(goToSymbol(QTreeWidgetItem*)));
      connect(symbols, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotShowContextMenu(QPoint)));

      // FIXME - lately this is broken and doesn't work anymore :(
      connect(win, SIGNAL(viewChanged()), this, SLOT(slotDocChanged()));
      //connect(symbols, SIGNAL(resizeEvent(QResizeEvent*)), this, SLOT(slotViewChanged(QResizeEvent*)));

      m_Active = true;
      //symbols->addColumn(i18n("Symbols"), symbols->parentWidget()->width());
      titles << i18nc("@title:column", "Symbols") << i18nc("@title:column", "Position");
      symbols->setColumnCount(2);
      symbols->setHeaderLabels(titles);

      symbols->setColumnHidden(1, true);
      symbols->setSortingEnabled(false);
      symbols->setRootIsDecorated(0);
      symbols->setContextMenuPolicy(Qt::CustomContextMenu);
      symbols->setIndentation(10);
      //symbols->setShowToolTips(true);

      /* First Symbols parsing here...*/
      parseSymbols();
     }
  else
     {
      delete dock;
      dock = 0;
      symbols = 0;
      m_Active = false;
     }
}

void KatePluginSymbolViewerView::slotRefreshSymbol()
{
  if (!symbols)
    return;
 symbols->clear();
 parseSymbols();
}

void KatePluginSymbolViewerView::slotChangeMode()
{
 treeMode = !treeMode;
 symbols->clear();
 parseSymbols();
}

void KatePluginSymbolViewerView::slotEnableSorting()
{
 lsorting = !lsorting;
 popup->setItemChecked(m_sort, lsorting);
 symbols->clear();
 if (lsorting == true)
     symbols->setSortingEnabled(true);
 else
     symbols->setSortingEnabled(false);

 parseSymbols();
 if (lsorting == true) symbols->sortItems(0, Qt::AscendingOrder);
}

void KatePluginSymbolViewerView::slotDocChanged()
{
 //kDebug(13000)<<"Document changed !!!!";
 slotRefreshSymbol();
}

void KatePluginSymbolViewerView::slotViewChanged(QResizeEvent *)
{
 //kDebug(13000)<<"View changed !!!!";
 symbols->setColumnWidth(0, symbols->parentWidget()->width());
}

void KatePluginSymbolViewerView::slotShowContextMenu(const QPoint &p)
{
 popup->popup(symbols->mapToGlobal(p));
}

void KatePluginSymbolViewerView::parseSymbols(void)
{
  QTreeWidgetItem *node = NULL;

  if (!win->activeView())
    return;

  KTextEditor::Document *doc = win->activeView()->document();

  // be sure we have some document around !
  if (!doc)
    return;

  /** Get the current highlighting mode */
  QString hlModeName = doc->mode();

  if (hlModeName == "C++" || hlModeName == "C" || hlModeName == "ANSI C89")
     parseCppSymbols();
 else if (hlModeName == "PHP (HTML)")
    parsePhpSymbols();
  else if (hlModeName == "Tcl/Tk")
     parseTclSymbols();
  else if (hlModeName == "Fortran")
     parseFortranSymbols();
  else if (hlModeName == "Perl")
     parsePerlSymbols();
  else if (hlModeName == "Python")
     parsePythonSymbols();
 else if (hlModeName == "Ruby")
    parseRubySymbols();
  else if (hlModeName == "Java")
     parseCppSymbols();
  else if (hlModeName == "xslt")
     parseXsltSymbols();
  else if (hlModeName == "Bash")
     parseBashSymbols();
  else
     node = new QTreeWidgetItem(symbols,  QStringList(i18n("Sorry. Language not supported yet") ) );
}

void KatePluginSymbolViewerView::goToSymbol(QTreeWidgetItem *it)
{
  KTextEditor::View *kv = win->activeView();

  // be sure we really have a view !
  if (!kv)
    return;

  kDebug(13000)<<"Slot Activated at pos: "<<symbols->indexOfTopLevelItem(it);

  kv->setCursorPosition (KTextEditor::Cursor (it->text(1).toInt(NULL, 10), 0));
}

KatePluginSymbolViewer::KatePluginSymbolViewer( QObject* parent, const QList<QVariant>& )
    : Kate::Plugin ( (Kate::Application*)parent, "katesymbolviewerplugin" )
{
 kDebug(13000)<<"KatePluginSymbolViewer";
}

KatePluginSymbolViewer::~KatePluginSymbolViewer()
{
  kDebug(13000)<<"~KatePluginSymbolViewer";
}

Kate::PluginView *KatePluginSymbolViewer::createView (Kate::MainWindow *mainWindow)
{
  KatePluginSymbolViewerView2 *view = new KatePluginSymbolViewerView2 (mViews, mainWindow);
  return view;
  //return new KatePluginSymbolViewerView2 (mainWindow);
}

Kate::PluginConfigPage* KatePluginSymbolViewer::configPage(
    uint, QWidget *w, const char* /*name*/)
{
  KatePluginSymbolViewerConfigPage* p = new KatePluginSymbolViewerConfigPage(this, w);

  KConfigGroup config(KGlobal::config(), "PluginSymbolViewer"); 
  p->viewReturns->setChecked(config.readEntry("ViewTypes", false));
  p->expandTree->setChecked(config.readEntry("ExpandTree", false));
  connect( p, SIGNAL(configPageApplyRequest(KatePluginSymbolViewerConfigPage*)),
      SLOT(applyConfig(KatePluginSymbolViewerConfigPage*)) );
  return (Kate::PluginConfigPage*)p;
}

KIcon KatePluginSymbolViewer::configPageIcon (uint number) const
{
  QPixmap cls( ( const char** ) class_xpm );
  if (number != 0) return KIcon();
  return KIcon(cls);
}

void KatePluginSymbolViewer::applyConfig( KatePluginSymbolViewerConfigPage* p )
{
  KConfigGroup config(KGlobal::config(), "PluginSymbolViewer");
  config.writeEntry("ViewTypes", p->viewReturns->isChecked());
  config.writeEntry("ExpandTree", p->expandTree->isChecked());

 for (int z=0; z < mViews.count(); z++)
  {
    mViews.at(z)->types_on = KConfigGroup(KGlobal::config(), "PluginSymbolViewer").readEntry("ViewTypes", false);
    mViews.at(z)->expanded_on = KConfigGroup(KGlobal::config(), "PluginSymbolViewer").readEntry("ExpandTree", false);
    mViews.at(z)->slotRefreshSymbol();
    //kDebug(13000)<<"KatePluginSymbolViewer: Configuration applied.("<<mViews.at(z)->types_on<<")";
  }
}

// BEGIN KatePluginSymbolViewerConfigPage
KatePluginSymbolViewerConfigPage::KatePluginSymbolViewerConfigPage(
    QObject* /*parent*/ /*= 0L*/, QWidget *parentWidget /*= 0L*/)
  : Kate::PluginConfigPage( parentWidget )
{
  QVBoxLayout *lo = new QVBoxLayout( this );
  int spacing = KDialog::spacingHint();
  lo->setSpacing( spacing );

  QGroupBox* groupBox = new QGroupBox( i18n("Parser Options"), this);

  viewReturns = new QCheckBox(i18n("Display functions parameters"));
  expandTree = new QCheckBox(i18n("Automatically expand nodes in tree mode"));

  QVBoxLayout* top = new QVBoxLayout();
  top->addWidget(viewReturns);
  top->addWidget(expandTree);
  groupBox->setLayout(top);
  lo->addWidget( groupBox );
  lo->addStretch( 1 );


//  throw signal changed
  connect(viewReturns, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
  connect(expandTree, SIGNAL(toggled(bool)), this, SIGNAL(changed()));
}

KatePluginSymbolViewerConfigPage::~KatePluginSymbolViewerConfigPage() {}

void KatePluginSymbolViewerConfigPage::apply()
{
  emit configPageApplyRequest( this );

}
// END KatePluginSymbolViewerConfigPage

