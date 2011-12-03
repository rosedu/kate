 /***************************************************************************
                           plugin_katexmlcheck.h
                           -------------------
	begin                : 2002-07-06
	copyright            : (C) 2002 by Daniel Naber
	email                : daniel.naber@t-online.de
 ***************************************************************************/

/***************************************************************************
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 ***************************************************************************/

#ifndef PLUGIN_KATEXMLCHECK_H
#define PLUGIN_KATEXMLCHECK_H

#include "xml_parser_parts.h"
#include <q3listview.h>
#include <qstring.h>
#include <QProcess>
//Added by qt3to4:
#include <Q3PtrList>
#include <QStack>
#include <QSet>
#include<QHash>
#include <kate/plugin.h>
#include <kate/application.h>
#include <kate/documentmanager.h>
#include <kate/mainwindow.h>

#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/markinterface.h>
#include <ktexteditor/movingrange.h>
#include <ktexteditor/movinginterface.h>

#include <k3dockwidget.h>
#include <kiconloader.h>
#include <QVariantList>



class KTemporaryFile;
class KProcess;

class PluginKateXMLEditView : public Kate::PluginView, public Kate::XMLGUIClient
{
    Q_OBJECT

public:
    PluginKateXMLEditView(Kate::MainWindow *mainwin);
    virtual ~PluginKateXMLEditView();

    Kate::MainWindow *win;
    QWidget *dock;

public slots:
//ADDED
    void documentChanged();
    /**
      *rangeList returns a Qvector with all XML Valid tags
      *within the range range
      *It uses a basic regex, just for testing
    */
    QVector<KTextEditor::Range> rangesList(KTextEditor::Document *doc, KTextEditor::Range range);
    QVector<KTextEditor::Range> wrongRangesResult(KTextEditor::Document *doc, QVector<KTextEditor::Range> vec);
    /**
        Search for a QString in a stack of ranges
        O(n) complexity
    */
    bool findQStringInStack(KTextEditor::Document *doc, QStack<KTextEditor::Range> ranges, QString s);
    /**
    Hilight all the ranges from Qvector ranges
    */
    void hilightSeveralRanges(KTextEditor::Document *doc, QVector<KTextEditor::Range> ranges);
    void resetHighlight();
    /**
      *hilight the rng range from the document dcm
    */
    void highlightRange(KTextEditor::Document* dcm, KTextEditor::Range rng);
    void characterChanged();
    void generalWrongRanges(QVector<KTextEditor::Range> vec, KTextEditor::Range selectiveRange);
//

private:
    QVector<KTextEditor::MovingRange*> m_hilightedRanges;
    QVector<KTextEditor::Range> m_wrongRanges;

};


class PluginKateXMLEdit : public Kate::Plugin
{
  Q_OBJECT

public:
  explicit PluginKateXMLEdit( QObject* parent = 0, const QVariantList& = QVariantList() );
    virtual ~PluginKateXMLEdit();
    Kate::PluginView *createView(Kate::MainWindow *mainWindow);
};

#endif // PLUGIN_KATEXMLCHECK_H




