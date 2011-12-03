/***************************************************************************
                           plugin_katexmlcheck.cpp - checks XML files using xmllint
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

/*
-fixme: show dock if "Validate XML" is selected (doesn't currently work when Kate
 was just started and the dockwidget isn't yet visible)
-fixme(?): doesn't correctly disappear when deactivated in config
*/

#include "plugin_katexmlcheck.h"
#include <QHBoxLayout>
#include "plugin_katexmlcheck.moc"

#include <cassert>
#include <QTextDocumentWriter>
#include <qfile.h>
#include <qinputdialog.h>
#include <qregexp.h>
#include <qstring.h>
#include <qtextstream.h>
#include <kactioncollection.h>
//Added by qt3to4:
#include <Q3CString>
#include <QApplication>

#include <ktexteditor/movingrange.h>
#include <ktexteditor/movinginterface.h>

#include <ktexteditor/highlightinterface.h>
#include <kdefakes.h> // for setenv
#include <kaction.h>
#include <kcursor.h>
#include <kdebug.h>
#include <k3dockwidget.h>
#include <kcomponentdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <ktemporaryfile.h>
#include <kpluginfactory.h>
#include <kprocess.h>


K_PLUGIN_FACTORY(PluginKateXMLCheckFactory, registerPlugin<PluginKateXMLCheck>();)
K_EXPORT_PLUGIN(PluginKateXMLCheckFactory("katexmlcheck"))

PluginKateXMLCheck::PluginKateXMLCheck( QObject* parent, const QVariantList& )
	: Kate::Plugin ( (Kate::Application *)parent )
{
}


PluginKateXMLCheck::~PluginKateXMLCheck()
{
}


Kate::PluginView *PluginKateXMLCheck::createView(Kate::MainWindow *mainWindow)
{
    return new PluginKateXMLCheckView(mainWindow);
}
//i18n(char *s)->afiseaza 
//---------------------------------
PluginKateXMLCheckView::PluginKateXMLCheckView(Kate::MainWindow *mainwin)//se executa prima
    : Kate::PluginView (mainwin), Kate::XMLGUIClient(PluginKateXMLCheckFactory::componentData()),win(mainwin)
{

    dock = win->createToolView("kate_plugin_xmlcheck_ouputview", Kate::MainWindow::Bottom, SmallIcon("misc"), i18n("XML Checker Output"));// Kate::MainWindow *win;

/*
QWidget * Kate::MainWindow::createToolView 	( 	const QString &  	identifier,
		MainWindow::Position  	pos,
		const QPixmap &  	icon,
		const QString &  	text	 
	) 
*/


    listview = new Q3ListView( dock );
    m_tmp_file=0;
    m_proc=0;
    QAction *a = actionCollection()->addAction("xml_check");
    a->setText(i18n("Validate XML"));
    connect(a, SIGNAL(triggered()), this, SLOT(slotValidate()));//fiecare conexiune se executa o singura data
    // TODO?:
    //(void)  new KAction ( i18n("Indent XML"), KShortcut(), this,
    //	SLOT(slotIndent()), actionCollection(), "xml_indent" );

    listview->setFocusPolicy(Qt::NoFocus);
    listview->addColumn(i18n("#"), -1);
    listview->addColumn(i18n("Line"), -1);
    listview->setColumnAlignment(1, Qt::AlignRight);
    listview->addColumn(i18n("Column"), -1);
    listview->setColumnAlignment(2, Qt::AlignRight);
    listview->addColumn(i18n("Message"), -1);
    listview->setAllColumnsShowFocus(true);
    listview->setResizeMode(Q3ListView::LastColumn);

    connect(listview, SIGNAL(clicked(Q3ListViewItem*)), SLOT(slotClicked(Q3ListViewItem*)));

    QList<KTextEditor::Document*> lst = Kate::application()->documentManager()->documents();
    KTextEditor::Document *dcm=lst[0];
    connect(dcm, SIGNAL(modifiedChanged(KTextEditor::Document*)), this, SLOT(documentChanged()) );

 //TODO?: invalidate the listview when document has changed
/*
   KTextEditor::View *kv = win->activeView();

   if( ! kv ) {
   kDebug() << "\n\nWarning: no Kate::View\n\n";
   return;
   }

   // kDebug()<<"\n\nConnection begin\n\n";
    connect(kv, SIGNAL(cursorPositionChanged(KTextEditor::View*,KTextEditor::Cursor)), this, SLOT(documentChanged()));
    //kDebug()<<"\n\nConnection end\n\n";
*/
    m_proc = new KProcess();//KProcess= QProcess
    connect(m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(slotProcExited(int,QProcess::ExitStatus)));
    // we currently only want errors:
    m_proc->setOutputChannelMode(KProcess::OnlyStderrChannel);

    mainWindow()->guiFactory()->addClient(this);


}


PluginKateXMLCheckView::~PluginKateXMLCheckView()//destructor
{
    mainWindow()->guiFactory()->removeClient( this );
    delete m_proc;
    delete m_tmp_file;
}


void PluginKateXMLCheckView::slotProcExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);//macro pentru evitarea warrningurilor cu avertismente de nefolosire a unor variabile;
;

    // FIXME: doesn't work correct the first time:
    //if( m_dockwidget->isDockBackPossible() ) {
    //	m_dockwidget->dockBack();
//	}

    if (exitStatus != QProcess::NormalExit) {//QProcess::NormalExit enum, value=0
        (void)new Q3ListViewItem(listview, QString("1").rightJustified(4,' '), "", "",
                                 "Validate process crashed.");
//Q3ListViewItem e o functie care contine un rand cu mai multe coloane care poate fi listata in Q3ListView functia asta apare  in cazul
//unui crash,e mai greu de testat
        return;
    }

    kDebug() << "slotProcExited()";

    QApplication::restoreOverrideCursor();//seteaza setul cursorului la cel original

    delete m_tmp_file;

    QString proc_stderr = QString::fromLocal8Bit(m_proc->readAllStandardError());//fromLocal8Bit returneaza un string format din caractere pe primii 8 biti ai m_proc->readAllStandardError()
//care e diferenta intre QProcess si Kprocess? 
//QProcess::readAllStandardArray e un QByteArray 
    m_tmp_file=0;
    listview->clear();
    uint list_count = 0;
    uint err_count = 0; 
	

    if( ! m_validating ) {
        // no i18n here, so we don't get an ugly English<->Non-english mixup:
        QString msg;
        if( m_dtdname.isEmpty() ) {
            msg = "No DOCTYPE found, will only check well-formedness.";
        } else {
            msg = '\'' + m_dtdname + "' not found, will only check well-formedness.";
        }
        (void)new Q3ListViewItem(listview, QString("1").rightJustified(4,' '), "", "", msg);//introduce mesajul in lista listview in ordinea argumentului 2 
        list_count++;//list_count : numarul de elemente din lista

    }

    if( ! proc_stderr.isEmpty() ) {//daca au fost erori(not shure) 
        QStringList lines = QStringList::split("\n", proc_stderr);//QstringList->lista de stringuri

        Q3ListViewItem *item = 0;
        QString linenumber, msg;
        int line_count = 0;

        for(QStringList::Iterator it = lines.begin(); it != lines.end(); ++it) {
            QString line = *it;
            line_count++;
            int semicolon_1 = line.find(':');
            int semicolon_2 = line.find(':', semicolon_1+1);
            int semicolon_3 = line.find(':', semicolon_2+2);
            int caret_pos = line.find('^');

            if( semicolon_1 != -1 && semicolon_2 != -1 && semicolon_3 != -1 ) {
                linenumber = line.mid(semicolon_1+1, semicolon_2-semicolon_1-1).trimmed();
                linenumber = linenumber.rightJustified(6, ' ');	// for sorting numbers
                msg = line.mid(semicolon_3+1, line.length()-semicolon_3-1).trimmed();
            } else if( caret_pos != -1 || line_count == lines.size() ) {
                // TODO: this fails if "^" occurs in the real text?!
                if( line_count == lines.size() && caret_pos == -1 ) {
                    msg = msg+'\n'+line;
                }
                QString col = QString::number(caret_pos);
                if( col == "-1" ) {
                    col = "";
                }
                err_count++;
                list_count++;
                item = new Q3ListViewItem(listview, QString::number(list_count).rightJustified(4,' '), linenumber, col, msg);
                item->setMultiLinesEnabled(true);
            } else {
                msg = msg+'\n'+line;
            }
        }
        listview->sort();	// TODO?: insert in right order
    }
    if( err_count == 0 ) {
        QString msg;
        if( m_validating ) {
            msg = "No errors found, document is valid.";	// no i18n here
        } else {
            msg = "No errors found, document is well-formed.";	// no i18n here
        }
        (void)new Q3ListViewItem(listview, QString::number(list_count+1).rightJustified(4,' '), "", "", msg);
    }
}


void PluginKateXMLCheckView::slotClicked(Q3ListViewItem *item)
{

	kDebug() << "slotClicked";

	if( item ) {
		bool ok = true;
		uint line = item->text(1).toUInt(&ok);
		bool ok2 = true;
		uint column = item->text(2).toUInt(&ok);
		if( ok && ok2 ) {
			KTextEditor::View *kv = win->activeView();
			if( ! kv )
				return;

			kv->setCursorPosition(KTextEditor::Cursor (line-1, column));
		}
	}
}


void PluginKateXMLCheckView::slotUpdate()
{

	
	kDebug() << "slotUpdate() executed"; 


}

void PluginKateXMLCheckView::documentChanged()
{
	KTextEditor::View *kv = win->activeView();
	
	if( ! kv ) {
   		kDebug() << "\n\nWarning: no Kate::View\n\n";
   		return;
   	}

   	connect(kv, SIGNAL(cursorPositionChanged(KTextEditor::View*,KTextEditor::Cursor)), this, SLOT(cursorPositionChanged()));

	
}
void PluginKateXMLCheckView::cursorPositionChanged() const
{
	
	//connect(Kate::application()->documentManager(), SIGNAL(documentCreated(KTextEditor::Document*)),

         //this, SLOT(slotDocumentCreated(KTextEditor::Document*)));
	//KTextEditor::View *kv = win->activeView();
	
	QList<KTextEditor::Document*> lst = Kate::application()->documentManager()->documents();
    	//KTextEditor::Document *dcm=lst[0];
	
	KTextEditor::Range *rng=new KTextEditor::Range();
	*rng = win->activeView()->document()->documentRange();

	KTextEditor::Cursor crs=win->activeView()->cursorPosition();
	if( crs.line() == 5 && crs.column()==4 )
	{
		
		win->activeView()->setCursorPosition( KTextEditor::Cursor(72,12) );
	
	}
	
	QApplication::restoreOverrideCursor();
	KTextEditor::View *kv = win->activeView();
	int i=0,j,k;//,aux,aux2;
	for(;i<kv->document()->lines();i++)
	{
		QString s=kv->document()->line(i);
		j=s.indexOf("<",0);
		k=s.indexOf(">",0);
		//kDebug()<<"\n"<<i<<" "<<j<<" "<<k<<"\n";
		//while(j!=-1 && k!=-1)
		//{
			if(j<k)
				highlightRange(lst[0],i,j,k-j+1);
		//	aux=k+1;
		///	j=s.indexOf("<",aux);
		//	k=s.indexOf(">",aux);
		//}
	}

	//kDebug()<<"\n"<<b<<"\n";
	//win->activeView()->document()->text();
	//if(rng->numberOfLines() > 1 && rng->columnWidth() > 3)
	//	highlightRange(lst[0],1,2,2);
	
}

void PluginKateXMLCheckView::highlightRange(KTextEditor::Document* dcm, int line, int column, int matchLen) const
{
	KTextEditor::MovingInterface* miface = qobject_cast<KTextEditor::MovingInterface*>(dcm);
	KTextEditor::Attribute::Ptr attr(new KTextEditor::Attribute());
	attr->setBackground(Qt::red);

	KTextEditor::Range range(line, column, line, column+matchLen);

	KTextEditor::MovingRange* mr = miface->newMovingRange(range);
	mr->setAttribute(attr);
        mr->setZDepth(-90000.0); // Set the z-depth to slightly worse than the selection
        mr->setAttributeOnlyForViews(true);

}

bool PluginKateXMLCheckView::slotValidate()//partea asta se executa cand este apasat butonul din MENIU "XML Validate", foloseste programul xmllint
{

	kDebug() << "slotValidate()";

	win->showToolView (dock);//afiseaza ce a fost creat cu createToolView
	
	m_proc->clearProgram();//KProcess clearProgram() sterge programul si argumentele din linia de comanda
	m_validating = false;
	m_dtdname = "";

	KTextEditor::View *kv = win->activeView();

	QList<KTextEditor::Document*> lst = Kate::application()->documentManager()->documents();//primul document
	KTextEditor::Range rng=lst[0]->documentRange();

	/*
  	KTextEditor::MovingInterface* moving = qobject_cast<KTextEditor::MovingInterface*>( kv->document() );
 
  	if ( moving ) {
      		KTextEditor::MovingRange* range = moving->newMovingRange(rng);
		kDebug()<< "\n\nRange works\n\n";
  	}
	else
	{
		kDebug()<< "\n\nRange does not work\n\n";
	}
*/

	
	 // doc is of type KTextEditor::Document*
/*
 	KTextEditor::HighlightInterface *iface =
     	qobject_cast<KTextEditor::HighlightInterface*>( lst[0] );

 	if( iface ) {
     		// the implementation supports the interface
     		// do stuff
	 }

*/
	//KTextEditor::Document *kd=win->document();

	//KTextEditor::Range *rng=new KTextEditor::Range();
	//rng = kv->document()->documentRange();

	//KTextEditor::Document::documentEnd(); returneaza ultima coloana de pe ultima linie din text


	QString d_contains;
	d_contains=kv->document()->text();

	kDebug() <<"\n\nText:"<<d_contains<<"\n\n";


	kDebug() <<"\n\n"<<kv->cursorPosition()<<"\n\n";

 

//

	if( ! kv )
	  return false;
        delete m_tmp_file;
	m_tmp_file = new KTemporaryFile();//KTemporaryFile la fel ca QTemporaryFile creaza un fisier temporar creaza un fisier temporar in QDir::tempPath() cu numele qt_temp.xxxxxx

	if( !m_tmp_file->open() ) {//daca nu a fputut fi creat fisierul temporar

		kDebug() << "Error (slotValidate()): could not create '" << m_tmp_file->fileName() << "': " << m_tmp_file->errorString();
		KMessageBox::error(0, i18n("<b>Error:</b> Could not create "
			"temporary file '%1'.", m_tmp_file->fileName()));
		delete m_tmp_file;
		m_tmp_file=0L;
		return false;
	}

	QTextStream s ( m_tmp_file );


	s << kv->document()->text();
	s.flush();
	kDebug() <<"\nNumele fisierului temporar:"<<m_tmp_file<<"\n";

	QString exe = KStandardDirs::findExe("xmllint");//cauta adresa executabilului xmllint
	
	if( exe.isEmpty() ) {//daca 
		
		exe = KStandardDirs::locate("exe", "xmllint");//mai cauta odata
	}

	// use catalogs for KDE docbook:
	if( ! getenv("XML_CATALOG_FILES") ) {
		KComponentData ins("katexmlcheckplugin");
		QString catalogs;
		catalogs += ins.dirs()->findResource("data", "ksgmltools2/customization/catalog.xml");
		kDebug() << "catalogs: " << catalogs;
		setenv("XML_CATALOG_FILES", QFile::encodeName( catalogs ).data(), 1);
	}
	//kDebug() << "**catalogs: " << getenv("XML_CATALOG_FILES");

	*m_proc << exe << "--noout";

	// tell xmllint the working path of the document's file, if possible.
	// otherweise it will not find relative DTDs
	QString path = kv->document()->url().directory();
	kDebug() << path;
	if (!path.isEmpty()) {
		*m_proc << "--path" << path;
	}

	// heuristic: assume that the doctype is in the first 10,000 bytes:
	QString text_start = kv->document()->text().left(10000);
	// remove comments before looking for doctype (as a doctype might be commented out
	// and needs to be ignored then):
	QRegExp re("<!--.*-->");//cauta expresii regulate in text, aici pentru excluderea commenturilor
	re.setMinimal(true);
	text_start.remove(re);
	QRegExp re_doctype("<!DOCTYPE\\s+(.*)\\s+(?:PUBLIC\\s+[\"'].*[\"']\\s+[\"'](.*)[\"']|SYSTEM\\s+[\"'](.*)[\"'])", false);
	re_doctype.setMinimal(true);

	if( re_doctype.search(text_start) != -1 ) {
		QString dtdname;
		if( ! re_doctype.cap(2).isEmpty() ) {
			dtdname = re_doctype.cap(2);
		} else {
			dtdname = re_doctype.cap(3);
		}
		if( !dtdname.startsWith("http:") ) {		// todo: u_dtd.isLocalFile() doesn't work :-(
			// a local DTD is used
			m_validating = true;
			*m_proc << "--valid";
		} else {
			m_validating = true;
			*m_proc << "--valid";
		}
	} else if( text_start.find("<!DOCTYPE") != -1 ) {
		// DTD is inside the XML file
		m_validating = true;
		*m_proc << "--valid";
	}
	*m_proc << m_tmp_file->fileName();

	m_proc->start();
	if( ! m_proc->waitForStarted(-1) ) {//asteapta pana incepe programul xmllint, daca nu incepe returneaza o eroare
		KMessageBox::error(0, i18n("<b>Error:</b> Failed to execute xmllint. Please make "
			"sure that xmllint is installed. It is part of libxml2."));
		return false;
	}
	QApplication::setOverrideCursor(Qt::WaitCursor);//imaginea cursorului de asteptare
	return true;
}

