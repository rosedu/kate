/* This file is part of the KDE project
   Copyright (C) 2008 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2010 Dominik Haumann <dhaumann kde org>

   In addition to the following license there is an exception, that the KDE e.V.
   may decide on other forms of relicensing.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "katecontainer.h"
#include "kateapp.h"
#include "katemainwindow.h"
#include "kateviewmanager.h"

#include <kdebug.h>

KateContainer::KateContainer(KateApp* parent)
  : QObject(parent)
  , KTextEditor::ViewBarContainer()
  , KTextEditor::MdiContainer()
  , m_app(parent)
{
}

KateContainer::~KateContainer() {
}

//BEGIN KTextEditor::ViewBarContainer
QWidget* KateContainer::getViewBarParent(KTextEditor::View *view,KTextEditor::ViewBarContainer::Position position) {
  if (position==KTextEditor::ViewBarContainer::BottomBar) {
    KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
    if (!mainWindow) {
      kDebug()<<"returning hardcoded 0, views window is not a KateMainWindow";
      return 0;
    }
    //Toplevel is a KateMainWindow
    return mainWindow->bottomViewBarContainer();
  } else if (position==KTextEditor::ViewBarContainer::TopBar) {
    KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
    if (!mainWindow) {
      kDebug()<<"returning hardcoded 0, views window is not a KateMainWindow";
      return 0;
    }
    //Toplevel is a KateMainWindow
    return mainWindow->topViewBarContainer();
  }
  return 0;
}

void KateContainer::addViewBarToLayout(KTextEditor::View *view,QWidget *bar, KTextEditor::ViewBarContainer::Position position) {
  KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
  if (!mainWindow) {
    kDebug()<<"main window is not a katemainwindow";
    return;
  }

  if (position==KTextEditor::ViewBarContainer::BottomBar) {
    mainWindow->addToBottomViewBarContainer(view,bar);
  } else   if (position==KTextEditor::ViewBarContainer::TopBar) {
    mainWindow->addToTopViewBarContainer(view,bar);
  }
    
}

void KateContainer::showViewBarForView(KTextEditor::View *view, KTextEditor::ViewBarContainer::Position position) {
  KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
    if (!mainWindow) {
      kDebug()<<"main window is not a katemainwindow";
      return;
  }
  
  if (position==KTextEditor::ViewBarContainer::BottomBar) {
    mainWindow->showBottomViewBarForView(view);
  } else if (position==KTextEditor::ViewBarContainer::TopBar) {
    mainWindow->showTopViewBarForView(view);
  }
}

void KateContainer::hideViewBarForView(KTextEditor::View *view, KTextEditor::ViewBarContainer::Position position) {
  KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
  if (!mainWindow) {
    kDebug()<<"main window is not a katemainwindow";
    return;
  }

  if (position==KTextEditor::ViewBarContainer::BottomBar) {
    mainWindow->hideBottomViewBarForView(view);      
  } else if (position==KTextEditor::ViewBarContainer::TopBar) {    
    mainWindow->hideTopViewBarForView(view);      
  }
}

void KateContainer::deleteViewBarForView(KTextEditor::View *view, KTextEditor::ViewBarContainer::Position position) {
  KateMainWindow* mainWindow=qobject_cast<KateMainWindow*>(view->window());
  if (!mainWindow) {
    kDebug()<<"main window is not a katemainwindow";
    return;
  }
  
  if (position==KTextEditor::ViewBarContainer::BottomBar) {      
    mainWindow->deleteBottomViewBarForView(view);
  } else if (position==KTextEditor::ViewBarContainer::TopBar) {      
    mainWindow->deleteTopViewBarForView(view);
  }
}
//END KTextEditor::ViewBarContainer



//BEGIN KTextEditor::MdiContainer
void KateContainer::setActiveView(KTextEditor::View *view)
{
  Q_UNUSED(view)
  // NOTE: not implemented
}

KTextEditor::View *KateContainer::activeView()
{
  return KateApp::self()->activeMainWindow()->viewManager()->activeView();
}

KTextEditor::Document *KateContainer::createDocument()
{
  // NOTE: not implemented
  kWarning() << "WARNING: interface call not implemented";
  return 0;
}

bool KateContainer::closeDocument(KTextEditor::Document *doc)
{
  Q_UNUSED(doc)
  // NOTE: not implemented
  kWarning() << "WARNING: interface call not implemented";
  return false;
}

KTextEditor::View *KateContainer::createView(KTextEditor::Document *doc)
{
  Q_UNUSED(doc)
  // NOTE: not implemented
  kWarning() << "WARNING: interface call not implemented";
  return 0;
}

bool KateContainer::closeView(KTextEditor::View *view)
{
  Q_UNUSED(view)
  // NOTE: not implemented
  kWarning() << "WARNING: interface call not implemented";
  return false;
}
//END KTextEditor::MdiContainer


#include "katecontainer.moc"
// kate: space-indent on; indent-width 2; replace-tabs on;
