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

#include "completionmodel.h"
#include "completionmodel.moc"
#include <kdebug.h>
#include <klocale.h>
#include <ktexteditor/templateinterface.h>
#include <ktexteditor/highlightinterface.h>
#include <ktexteditor/codecompletioninterface.h>
#include <ktexteditor/view.h>
#include <kmessagebox.h>
#include <kshortcut.h>
#include <kaction.h>
#include <kactioncollection.h>
#ifdef SNIPPET_EDITOR
  #include <kstandarddirs.h>
  #include <kglobal.h>
  #include <qfileinfo.h>
#endif
#include <qapplication.h>
#include <QDomDocument>
#include <QFile>

#define COMPLETION_THRESHOLD 3

namespace KTextEditor {
  namespace CodesnippetsCore {
#ifdef SNIPPET_EDITOR
  namespace Editor {
#endif
    class SnippetCompletionEntry {
      public:
        SnippetCompletionEntry(const QString& _match,const QString& _prefix, const QString& _postfix, const QString& _arguments, const QString& _fillin, int _script=-1, const QString& _shortcut=QString()):
          match(_match),prefix(_prefix),postfix(_postfix),arguments(_arguments),fillin(_fillin),shortcut(_shortcut),script(_script){
            //kDebug()<<"SnippetCompletionEntry: shortcut"<<shortcut;
          }
        ~SnippetCompletionEntry(){};
        QString match;
        QString prefix;
        QString postfix;
        QString arguments;
        QString fillin;
        QString shortcut;
        int     script;

        friend bool operator == (const SnippetCompletionEntry& first, const SnippetCompletionEntry &second);
    };

    bool operator == (const SnippetCompletionEntry& first, const SnippetCompletionEntry &second) {
          if (first.match!=second.match) return false;
          if (first.prefix!=second.prefix) return false;
          if (first.postfix!=second.postfix) return false;
          if (first.arguments!=second.arguments) return false;
          if (first.fillin!=second.fillin) return false;
          return true;          
        }
        
    class FilteredEntry {
      public:
        FilteredEntry(SnippetCompletionModel *_model, int _id):
          model(_model),id(_id){          
        }
      SnippetCompletionModel *model;
      int id;       
    };


  }
  }
#ifdef SNIPPET_EDITOR
  }
#endif

    
#include "internalcompletionmodel.h"
#include "internalcompletionmodel.moc"
    
namespace KTextEditor {
  namespace CodesnippetsCore {
#ifdef SNIPPET_EDITOR
  namespace Editor {
#endif
    
        
    
//BEGIN: SnippetCompletionModelPrivate
  class SnippetCompletionModelPrivate {
  public:
    SnippetCompletionModelPrivate(const QString &_fileType,const QStringList &_snippetFiles,TemplateScriptRegistrar *_scriptRegistrar):
    fileType(_fileType),
    mergedFiles(_snippetFiles),
    scriptRegistrar(_scriptRegistrar),
    automatic(false) {
    }

    QList<SnippetCompletionEntry> entries;
    QList<const SnippetCompletionEntry*> matches;
    QString fileType;
    QStringList mergedFiles;
    QList<KTextEditor::TemplateScript*> scripts;
    TemplateScriptRegistrar *scriptRegistrar;
    bool automatic;
  #ifdef SNIPPET_EDITOR
          QString script;
  #endif

      void loadEntries(QObject *scriptParent, const QString &filename) {
        int scriptID=-1;
    #ifndef SNIPPET_EDITOR
        QString script;
    #endif
        QFile f(filename);
        QDomDocument doc;
        if (f.open(QIODevice::ReadOnly)) {
          QString errorMsg;
          int line, col;
          bool success;
          success=doc.setContent(&f,&errorMsg,&line,&col);
          f.close();
          if (!success) {
            KMessageBox::error(QApplication::activeWindow(),i18n("<qt>The error <b>%4</b><br /> has been detected in the file %1 at %2/%3</qt>", filename,
                line, col, i18nc("QXml",errorMsg.toUtf8())));
            return;
          }
        } else {
          KMessageBox::error(QApplication::activeWindow(), i18n("Unable to open %1", filename) );
          return;
        }
        QDomElement el=doc.documentElement();
        if (el.tagName()!="snippets") {
          KMessageBox::error(QApplication::activeWindow(), i18n("Not a valid snippet file: %1", filename) );
          return;
        }
        QString nameSpace=el.attribute("namespace");
        QDomNodeList script_nodes=el.childNodes();
        for (int i_script=0;i_script<script_nodes.count();i_script++) {
          QDomElement script_node=script_nodes.item(i_script).toElement();
          if (script_node.tagName()=="script") {
            //kDebug()<<"Script tag has been found";
            script=script_node.firstChild().nodeValue();
            //kDebug()<<"Script is:"<< script;
            if (scriptRegistrar) {
              KTextEditor::TemplateScript* templateScript = scriptRegistrar->registerTemplateScript(scriptParent, script);
              //kDebug() << "Template Script pointer:" << templateScript;
              if (templateScript) {
                scripts.append(templateScript);
                scriptID=scripts.count()-1;
              }
            }
            break;
          }
        }


        QDomNodeList item_nodes=el.childNodes();

        QLatin1String match_str("match");
        QLatin1String prefix_str("displayprefix");
        QLatin1String postfix_str("displaypostfix");
        QLatin1String arguments_str("displayarguments");
        QLatin1String fillin_str("fillin");
        QLatin1String shortcut_str("shortcut");
        
        for (int i_item=0;i_item<item_nodes.count();i_item++) {
          QDomElement item=item_nodes.item(i_item).toElement();
          if (item.tagName()!="item") continue;

          QString match;
          QString prefix;
          QString postfix;
          QString arguments;
          QString fillin;
          QString shortcut;
          QDomNodeList data_nodes=item.childNodes();
          for (int i_data=0;i_data<data_nodes.count();i_data++) {
            QDomElement data=data_nodes.item(i_data).toElement();
            QString tagName=data.tagName();
            if (tagName==match_str)
              match=data.text();
            else if (tagName==prefix_str)
              prefix=data.text();
            else if (tagName==postfix_str)
              postfix=data.text();
            else if (tagName==arguments_str)
              arguments=data.text();
            else if (tagName==fillin_str)
              fillin=data.text();
            else if (tagName==shortcut_str)
              shortcut=data.text();
          }
          //kDebug(13040)<<prefix<<match<<postfix<<arguments<<fillin;
#ifdef SNIPPET_EDITOR
          entries.append(SnippetCompletionEntry(match,prefix,postfix,arguments,fillin,scriptID,shortcut));
#else
          entries.append(SnippetCompletionEntry(nameSpace+match,prefix,postfix,arguments,fillin,scriptID,shortcut));
#endif          
        }
      }


  };
//END: SnippetCompletionModelPrivate


//BEGIN: CompletionModel

    SnippetCompletionModel::SnippetCompletionModel(const QString &fileType, QStringList &snippetFiles,TemplateScriptRegistrar *scriptRegistrar):
      KTextEditor::CodeCompletionModel2((QObject*)0),
      d(new SnippetCompletionModelPrivate(fileType,snippetFiles,scriptRegistrar))
       {
        //kDebug()<<"About to load files "<<snippetFiles<<" for file type "<<fileType;
        foreach(const QString& str, snippetFiles) {
          d->loadEntries(this,str);
        }
    }

    SnippetCompletionModel::~SnippetCompletionModel() {
      delete d;
    }

#ifdef SNIPPET_EDITOR
    QString SnippetCompletionModel::script() { return d->script;}
    void SnippetCompletionModel::setScript(const QString& script) {d->script=script;}
#endif

    bool SnippetCompletionModel::loadHeader(const QString& filename, QString* name, QString* filetype, QString* authors, QString* license, QString* snippetlicense, QString* nameSpace) {
      //kDebug()<<filename;
      name->clear();
      filetype->clear();
      authors->clear();
      license->clear();
      nameSpace->clear();

      QFile f(filename);
      QDomDocument doc;
      if (f.open(QIODevice::ReadOnly)) {
        QString errorMsg;
        int line, col;
        bool success;
        success=doc.setContent(&f,&errorMsg,&line,&col);
        f.close();
        if (!success) {
          KMessageBox::error(QApplication::activeWindow(),i18n("<qt>The error <b>%4</b><br /> has been detected in the file %1 at %2/%3</qt>", filename,
              line, col, i18nc("QXml",errorMsg.toUtf8())));
          return false;
        }
      } else {
        KMessageBox::error(QApplication::activeWindow(), i18n("Unable to open %1", filename) );
        return false;
      }
      QDomElement el=doc.documentElement();
      if (el.tagName()!="snippets") {
        KMessageBox::error(QApplication::activeWindow(), i18n("Not a valid snippet file: %1", filename) );
        return false;
      }
      *name=el.attribute("name");
      *filetype=el.attribute("filetypes");
      *authors=el.attribute("authors");
      *license=el.attribute("license");
      *nameSpace=el.attribute("namespace");
      *snippetlicense=el.attribute("snippetlicense");      
      if (snippetlicense->isEmpty()) *snippetlicense=QString("public domain");
      return true;
    }



    SnippetSelectorModel *SnippetCompletionModel::selectorModel() {
      return new SnippetSelectorModel(this);
    }

    QString SnippetCompletionModel::fileType() {
        return d->fileType;
    }

    void SnippetCompletionModel::completionInvoked(KTextEditor::View* view,
      const KTextEditor::Range& range, InvocationType invocationType) {
        //kDebug(13040);
        bool my_mode=true;
        KTextEditor::HighlightInterface *hli=qobject_cast<KTextEditor::HighlightInterface*>(view->document());
        if (hli)
        {
          //kDebug()<<"me: "<<d->fileType<<" current hl in file: "<<hli->highlightingModeAt(range.end());
          if (hli->highlightingModeAt(range.end())!=d->fileType) my_mode=false;
        }
        d->automatic=false;
        if (my_mode) {
          if (invocationType==AutomaticInvocation) {
            d->automatic=true;
            //KateView *v = qobject_cast<KateView*> (view);

            if (range.columnWidth() >= COMPLETION_THRESHOLD) {
              d->matches.clear();
              foreach(const SnippetCompletionEntry& entry,d->entries) {
                d->matches.append(&entry);
              }
              reset();
              //kDebug(13040)<<"matches:"<<m_matches.count();
            } else {
              d->matches.clear();
              reset();
            }
          } else {
            d->matches.clear();
              foreach(const SnippetCompletionEntry& entry,d->entries) {
                d->matches.append(&entry);
              }
              reset();
          }
        } else {
            d->matches.clear();
            reset();
        }
    }

    QVariant SnippetCompletionModel::data( const QModelIndex & index, int role) const {
      //kDebug(13040);
      if (role == KTextEditor::CodeCompletionModel::InheritanceDepth)
        return 1;
      if (!index.parent().isValid()) {
        if (role==Qt::DisplayRole) return i18n("Snippets");
        if (role==GroupRole) return Qt::DisplayRole;
        return QVariant();
      }
      if (role == Qt::DisplayRole) {
        if (index.column() == KTextEditor::CodeCompletionModel::Name ) {
          //kDebug(13040)<<"returning: "<<m_matches[index.row()]->match;
          return d->matches[index.row()]->match;
        } else if (index.column() == KTextEditor::CodeCompletionModel::Prefix ) {
          const QString& tmp=d->matches[index.row()]->prefix;
          if (!tmp.isEmpty()) return tmp;
        } else if (index.column() == KTextEditor::CodeCompletionModel::Postfix ) {
          const QString& tmp=d->matches[index.row()]->postfix;
          if (!tmp.isEmpty()) return tmp;
        } else if (index.column() == KTextEditor::CodeCompletionModel::Arguments ) {
          const QString& tmp=d->matches[index.row()]->arguments;
          if (!tmp.isEmpty()) return tmp;
        }
      }
      return QVariant();
    }

    QModelIndex SnippetCompletionModel::parent(const QModelIndex& index) const {
      if (index.internalId())
        return createIndex(0, 0, 0);
      else
        return QModelIndex();
    }

    QModelIndex SnippetCompletionModel::index(int row, int column, const QModelIndex& parent) const {
      if (!parent.isValid()) {
        if (row == 0)
          return createIndex(row, column, 0); //header  index
        else
          return QModelIndex();
      } else if (parent.parent().isValid()) //we only have header and children, no subheaders
        return QModelIndex();

      if (row < 0 || row >= d->matches.count() || column < 0 || column >= ColumnCount )
        return QModelIndex();

      return createIndex(row, column, 1); // normal item index
    }

    int SnippetCompletionModel::rowCount (const QModelIndex & parent) const {
      if (!parent.isValid() && !d->matches.isEmpty())
        return 1; //one toplevel node (group header)
      else if(parent.parent().isValid())
        return 0; //we don't have sub children
      else
        return d->matches.count(); // only the children
    }

    void SnippetCompletionModel::executeCompletionItem2(KTextEditor::Document* document,
      const KTextEditor::Range& word, const QModelIndex& index) const {
      //document->replaceText(word, m_matches[index.row()]->fillin);
      document->removeText(word);
      KTextEditor::View *view=document->activeView();
      if (!view) return;

      KTextEditor::TemplateInterface2 *ti2=qobject_cast<KTextEditor::TemplateInterface2*>(view);
      if (ti2) {
        int script=d->matches[index.row()]->script;
        ti2->insertTemplateText (word.start(), d->matches[index.row()]->fillin, QMap<QString,QString> (),
                                 (script==-1)?0:d->scripts[script]);
      } else {
        KTextEditor::TemplateInterface *ti=qobject_cast<KTextEditor::TemplateInterface*>(view);
        if (ti)
          ti->insertTemplateText (word.start(), d->matches[index.row()]->fillin, QMap<QString,QString> ());
        else {
          view->setCursorPosition(word.start());
          view->insertText (d->matches[index.row()]->fillin);
        }
      }
    }




  Range SnippetCompletionModel::completionRange(View* view, const Cursor &position)
  {
//      kDebug()<<position;
      Cursor end = position;

      QString text = view->document()->line(end.line());

      static QRegExp findWordEnd( "^([_:\\w]*)\\b" );

      Cursor start = end;
      for (int i=end.column()-1;i>=-1;i--) {
          if (i==-1) {
            start.setColumn(0);
            break;
          }
          QChar c=text.at(i);
          if (! (c.isLetter() || c.isNumber() || (c=='_') || (c==':')) ) {
            start.setColumn(i+1);
            break;
          }
      }

      if (findWordEnd.indexIn(text.mid(end.column())) >= 0)
          end.setColumn(end.column() + findWordEnd.cap(1).length());

//      kDebug()<<Range(start,end);
      return Range(start, end);
  }

bool SnippetCompletionModel::shouldStartCompletion(KTextEditor::View* view, const QString &insertedText, bool userInsertion, const KTextEditor::Cursor &position)
{
    if (!userInsertion) return false;
    if(insertedText.isEmpty())
        return false;

    bool my_mode=true;
    KTextEditor::HighlightInterface *hli=qobject_cast<KTextEditor::HighlightInterface*>(view->document());
    if (hli)
    {
      //kDebug()<<"me: "<<d->fileType<<" current hl in file: "<<hli->highlightingModeAt(position);
      if (hli->highlightingModeAt(position)!=d->fileType) my_mode=false;
    }

    if (!my_mode) return false;


    QString text = view->document()->line(position.line()).left(position.column());
    int start=text.length();
    int end=text.length()-COMPLETION_THRESHOLD;
    if (end<0) return false;
    for (int i=start-1;i>=end;i--) {
      QChar c=text.at(i);
      if (! (c.isLetter() || (c.isNumber()) || c=='_' || c==':') ) return false;
    }

    return true;
}

bool SnippetCompletionModel::shouldAbortCompletion(KTextEditor::View* view, const KTextEditor::Range &range, const QString &currentCompletion) {

    if (d->automatic) {
      if (currentCompletion.length()<COMPLETION_THRESHOLD) return true;
    }

    if(view->cursorPosition() < range.start() || view->cursorPosition() > range.end())
        return true; //Always abort when the completion-range has been left
      //Do not abort completions when the text has been empty already before and a newline has been entered

      static const QRegExp allowedText("^([\\w:_]*)");
  //    kDebug()<<!allowedText.exactMatch(currentCompletion);
      return !allowedText.exactMatch(currentCompletion);
}


  #ifdef SNIPPET_EDITOR
    static void addAndCreateElement(QDomDocument& doc, QDomElement& item, const QString& name, const QString &content)
    {
      QDomElement element=doc.createElement(name);
      element.appendChild(doc.createTextNode(content));
      item.appendChild(element);
    }

    bool SnippetCompletionModel::save(const QString& filename, const QString& name, const QString& license, const QString& filetype, const QString& authors,const QString& snippetlicense, const QString& nameSpace)
    {
  /*
      <snippets name="Testsnippets" filetype="*" authors="Joseph Wenninger" license="BSD">
        <item>
          <displayprefix>prefix</displayprefix>
          <match>test1</match>
          <displaypostfix>postfix</displaypostfix>
          <displayarguments>(param1, param2)</displayarguments>
          <fillin>This is a test</fillin>
        </item>
        <item>
          <match>testtemplate</match>
          <fillin>This is a test ${WHAT} template</fillin>
        </item>
      </snippets>
  */
      QDomDocument doc;
      QDomElement root=doc.createElement("snippets");
      root.setAttribute("name",name);
      root.setAttribute("filetypes",filetype);
      root.setAttribute("authors",authors);
      root.setAttribute("license",license);
      root.setAttribute("snippetlicense",snippetlicense);
      root.setAttribute("namespace",nameSpace);
      doc.appendChild(root);
      if (!d->script.isEmpty()) {
        addAndCreateElement(doc,root,"script",d->script);
      }
      foreach(const SnippetCompletionEntry& entry, d->entries) {
        QDomElement item=doc.createElement("item");
        addAndCreateElement(doc,item,"displayprefix",entry.prefix);
        addAndCreateElement(doc,item,"match",entry.match);
        addAndCreateElement(doc,item,"displaypostfix",entry.postfix);
        addAndCreateElement(doc,item,"displayarguments",entry.arguments);
        if (!entry.shortcut.isEmpty())
          addAndCreateElement(doc,item,"shortcut",entry.shortcut);
        addAndCreateElement(doc,item,"fillin",entry.fillin);
        root.appendChild(item);
      }
      //KMessageBox::information(0,doc.toString());
      QFileInfo fi(filename);
      QString outname=KGlobal::dirs()->locateLocal( "data", "ktexteditor_snippets/data/"+fi.fileName());
      if (filename!=outname) {
        QFileInfo fiout(outname);
  //      if (fiout.exists()) {
  // there could be cases that new new name clashes with a global file, but I guess it is not that often.
              bool ok=false;
              for (int i=0;i<1000;i++) {
                outname=KGlobal::dirs()->locateLocal( "data", "ktexteditor_snippets/data/"+QString("%1_").arg(i)+fi.fileName());
                QFileInfo fiout1(outname);
                if (!fiout1.exists()) {ok=true;break;}
              }
              if (!ok) {
                  KMessageBox::error(0,i18n("You have edited a data file not located in your personal data directory, but a suitable filename could not be generated for storing a clone of the file within your personal data directory."));
                  return false;
              } else KMessageBox::information(0,i18n("You have edited a data file not located in your personal data directory; as such, a renamed clone of the original data file has been created within your personal data directory."));
  //       } else
  //         KMessageBox::information(0,i18n("You have edited a data file not located in your personal data directory, creating a clone of the data file in your personal data directory"));
      }
      QFile outfile(outname);
      if (!outfile.open(QIODevice::WriteOnly)) {
        KMessageBox::error(0,i18n("Output file '%1' could not be opened for writing",outname));
        return false;
      }
      outfile.write(doc.toByteArray());
      outfile.close();
      return true;
    }

  QString SnippetCompletionModel::createNew(const QString& name, const QString& license,const QString& authors,const QString& filetypes) {
      QDomDocument doc;
      QDomElement root=doc.createElement("snippets");
      root.setAttribute("name",name);
      root.setAttribute("filetypes",filetypes.isEmpty()?"*":filetypes);
      root.setAttribute("authors",authors);
      root.setAttribute("license",license);
      root.setAttribute("snippetlicense",QString("public domain"));
      doc.appendChild(root);
      QString fileName=QUrl::toPercentEncoding(name)+QString(".xml");
      QString outname=KGlobal::dirs()->locateLocal( "data", "ktexteditor_snippets/data/"+fileName);
  #ifdef __GNUC__
  #warning add handling of conflicts with global names
  #endif
      QFileInfo fiout(outname);
      if (fiout.exists()) {
        bool ok=false;
        for (int i=0;i<1000;i++) {
          outname=KGlobal::dirs()->locateLocal( "data", "ktexteditor_snippets/data/"+QString("%1_").arg(i)+fileName);
          QFileInfo fiout1(outname);
          if (!fiout1.exists()) {ok=true;break;}
        }
        if (!ok) {
          KMessageBox::error(0,i18n("It was not possible to create a unique file name for the given snippet collection name"));
          return QString();
        }
      }
      QFile outfile(outname);
      if (!outfile.open(QIODevice::WriteOnly)) {
        KMessageBox::error(0,i18n("Output file '%1' could not be opened for writing",outname));
        return QString();
      }
      outfile.write(doc.toByteArray());
      outfile.close();

      return outname;

  }

  #endif

//END: CompletionModel

//BEGIN: SelectorModel
    SnippetSelectorModel::SnippetSelectorModel(SnippetCompletionModel* cmodel):
      QAbstractItemModel(cmodel),m_cmodel(cmodel)
    {
      //kDebug(13040);
    }


    SnippetSelectorModel::~SnippetSelectorModel()
    {
      //kDebug(13040);
    }

    QString SnippetSelectorModel::fileType() {
        return m_cmodel->fileType();
    }

    int SnippetSelectorModel::rowCount(const QModelIndex& parent) const
    {
      if (parent.isValid()) return 0;
      return m_cmodel->d->entries.count();
    }

    QList<QAction*> SnippetSelectorModel::actions() {
      QList<QAction*> as;
      for (int i=0;i<rowCount(QModelIndex());i++) {
            QString shortcut;
            shortcut=m_cmodel->d->entries[i].shortcut;            
            if (shortcut.isEmpty()) continue;
            //kDebug()<<"Shortcut for snippet: "<<shortcut;
            KAction *a=new KAction(0);
            a->setObjectName(shortcut);
            a->setShortcut(KShortcut(shortcut));
            a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
            as.append(a);            
      }
      return as;
    }
    
    QModelIndex SnippetSelectorModel::index ( int row, int column, const QModelIndex & parent ) const
    {
      if (parent.isValid()) return QModelIndex();
      if (column!=0) return QModelIndex();
      if ((row>=0) && (row<m_cmodel->d->entries.count()))
        return createIndex(row,column);
      return QModelIndex();
    }

    QVariant SnippetSelectorModel::data(const QModelIndex &index, int role) const
    {
        if (role==MergedFilesRole) return m_cmodel->d->mergedFiles;
        if (!index.isValid()) return QVariant();
        switch (role) {
          case Qt::DisplayRole:
            return m_cmodel->d->entries[index.row()].match;
            break;
          case FillInRole:
            return m_cmodel->d->entries[index.row()].fillin;
            break;
          case ScriptTokenRole: {
            int script=m_cmodel->d->entries[index.row()].script;
            if (script==-1) return QString();
            return qVariantFromValue<void*>( (void*)m_cmodel->d->scripts[script]);
            break;
          }
  #ifdef SNIPPET_EDITOR
          case PrefixRole:
            return m_cmodel->d->entries[index.row()].prefix;
            break;
          case MatchRole:
            return m_cmodel->d->entries[index.row()].match;
            break;
          case PostfixRole:
            return m_cmodel->d->entries[index.row()].postfix;
            break;
          case ArgumentsRole:
            return m_cmodel->d->entries[index.row()].arguments;
            break;
          case ShortcutRole:
            return m_cmodel->d->entries[index.row()].shortcut;
            break;
  #endif
          default:
            break;
        }
        return QVariant();
    }

    QVariant SnippetSelectorModel::headerData ( int section, Qt::Orientation orientation, int role) const
    {
      if (orientation==Qt::Vertical) return QVariant();
      if (section!=0) return QVariant();
      if (role!=Qt::DisplayRole) return QVariant();
      return QString("Snippet");
    }


  #ifdef SNIPPET_EDITOR
    bool SnippetSelectorModel::setData ( const QModelIndex & index, const QVariant & value, int role) {
      if (!index.isValid()) return false;
      switch (role) {
          case FillInRole:
            m_cmodel->d->entries[index.row()].fillin=value.toString();
            break;
          case PrefixRole:
            m_cmodel->d->entries[index.row()].prefix=value.toString();
            break;
          case MatchRole:
            m_cmodel->d->entries[index.row()].match=value.toString();
            dataChanged(index,index);
            break;
          case PostfixRole:
            m_cmodel->d->entries[index.row()].postfix=value.toString();
            break;
          case ArgumentsRole:
            m_cmodel->d->entries[index.row()].arguments=value.toString();
            break;
          case ShortcutRole:
            m_cmodel->d->entries[index.row()].shortcut=value.toString();
            break;
        default:
          return QAbstractItemModel::setData(index,value,role);
      }
      return true;
    }

    bool SnippetSelectorModel::removeRows ( int row, int count, const QModelIndex & parent) {
      if (parent.isValid()) return false;
      if (count!=1) return false;
      if (row>=rowCount(QModelIndex())) return false;
      beginRemoveRows(parent,row,row);
      m_cmodel->d->entries.removeAt(row);
      endRemoveRows();
      return true;
    }

    QModelIndex SnippetSelectorModel::newItem() {
      int new_row=m_cmodel->d->entries.count();
      beginInsertRows(QModelIndex(),new_row,new_row);
      m_cmodel->d->entries.append(SnippetCompletionEntry(i18n("New Snippet"),QString(),QString(),QString(),QString()));
      endInsertRows();
      return createIndex(new_row,0);
    }
  #endif
  
    void SnippetSelectorModel::entriesForShortcut(const QString &shortcut, void* list) {
      QList<FilteredEntry> *l=(QList<FilteredEntry>*)list;
      for (int i=0;i<m_cmodel->d->entries.count();i++) {
        if (m_cmodel->d->entries[i].shortcut==shortcut) {
          l->append(FilteredEntry(m_cmodel,i));
        }
      }
    }
  
//END: SelectorModel




//BEGIN: CategorizedSnippetModel
        CategorizedSnippetModel::CategorizedSnippetModel(const QList<SnippetSelectorModel*>& models):
          QAbstractItemModel(),m_models(models),m_actionCollection(new KActionCollection(this)) {
            foreach(SnippetSelectorModel *model,m_models) {
              connect(model, SIGNAL(destroyed(QObject*)),this,SLOT(subDestroyed(QObject*)));
              foreach(QAction *a,model->actions()) {
              if (m_actionCollection->action(a->objectName())) {
                delete a;
              } else {
                a->setParent(this);
                m_actionCollection->addAction(a->objectName(),a);
                connect(a,SIGNAL(triggered()),this,SLOT(actionTriggered()));
              }
            }
          }
        }

        CategorizedSnippetModel::~CategorizedSnippetModel() {
          qDeleteAll(m_models);
        }

      
        bool lessThanFilteredEntry(const FilteredEntry& first,const FilteredEntry& second) {
          return ((first.model->d->entries[first.id]).match<(second.model->d->entries[second.id].match));
        }

        void CategorizedSnippetModel::actionTriggered() {          
          //KMessageBox::error(0,QString("Action triggered:%1").arg(sender()->objectName()));
          QList<FilteredEntry> entries;
          foreach (SnippetSelectorModel *model, m_models) {
            model->entriesForShortcut(sender()->objectName(), &entries);
          }
          //kDebug()<<"Found entries:"<<entries.count();
          if (entries.count()>1) {
            qSort(entries.begin(),entries.end(),lessThanFilteredEntry);
            //remove duplicates
            for (QList<FilteredEntry>::iterator it=entries.begin();it!=entries.end();) {
              QList<FilteredEntry>::iterator it1=it+1;
              SnippetCompletionEntry e1=it->model->d->entries[it->id];
              ++it;
              while (it1!=entries.end()) {
                SnippetCompletionEntry e2=it1->model->d->entries[it1->id];
                if ( e1 == e2) {
                  it1=entries.erase(it1);                  
                  it=it1;
                  //kDebug()<<"removed an entry";
                } else {
                  //kDebug()<<"breaking out of inner loop";
                  break;
                }
              }
            }
          }
          
          if (entries.count()==0) return;
          if (entries.count()==1) {
              SnippetCompletionModel *m=entries[0].model;
              KTextEditor::View* view;
              view=0;
              needView(&view);
              if (view==0) return;
              /*m->executeCompletionItem2(view->document(),
                KTextEditor::Range(view->cursorPosition(),view->cursorPosition()),
                m->index(entries[0].id,0,QModelIndex()));*/
              //kDebug()<<entries[0].id;
              
              QString fillin=m->d->entries[entries[0].id].fillin;
              int script=m->d->entries[entries[0].id].script;
              TemplateScript * ts=0;
              if (script!=-1)
                ts=m->d->scripts[script];
            
              //kDebug()<<"snippet content to insert is:"<<fillin;
              
              KTextEditor::TemplateInterface2 *ti2=qobject_cast<KTextEditor::TemplateInterface2*>(view);
              if (ti2) {

                ti2->insertTemplateText (view->cursorPosition(), fillin,QMap<QString,QString>(),ts);
              } else {
                KTextEditor::TemplateInterface *ti=qobject_cast<KTextEditor::TemplateInterface*>(view);
                if (ti)
                  ti->insertTemplateText (view->cursorPosition(), fillin,QMap<QString,QString>());
              }
              view->setFocus();
              return;
          } else { 
            // MORE THEN ONE FOUND
            KTextEditor::View* view;
            view=0;
            needView(&view);
            if (view==0) return;
            
            KTextEditor::CodeCompletionInterface *iface =
            qobject_cast<KTextEditor::CodeCompletionInterface*>( view );
            if (iface) {
              iface->startCompletion(Range(view->cursorPosition(),view->cursorPosition()), new InternalCompletionModel(entries));
            }            
          }
        }

        void CategorizedSnippetModel::subDestroyed(QObject* obj) {
          int i=m_models.indexOf((SnippetSelectorModel*)(obj));
          if (i==-1) return;
          m_models.removeAt(i);
          reset();
        }

        int CategorizedSnippetModel::rowCount(const QModelIndex& parent) const {
          if (!parent.isValid())
          {
            return m_models.count();
          } else {
//             kDebug()<<"Requested rowCount with valid parent";
//             kDebug()<<"parent.internalPointer"<<parent.internalPointer();
//             kDebug()<<"parent.internalId"<<parent.internalId();
            if (!parent.internalPointer()) {
              SnippetSelectorModel* m=m_models[parent.row()];
//               kDebug()<<"Returning child rowCount:"<<m->rowCount(QModelIndex());
              return m->rowCount(QModelIndex());
            }
          }
          return 0;
        }

        QModelIndex CategorizedSnippetModel::index ( int row, int column, const QModelIndex & parent) const
        {
          if (row==-1) {
//               kDebug()<<"row==-1 -> returning invalid index"<<endl;
              return QModelIndex();
          }
          if (parent.isValid()) {
//             kDebug()<<"Requested index for valid parent"<<endl;
            if ((column==0) && ((row>=0) && (row<m_models[parent.row()]->rowCount(QModelIndex()))))
            {
              QModelIndex idx=createIndex(row,column,m_models[parent.row()]);
              //kDebug()<<idx;
              return idx;
            }
          } else  {
//               kDebug()<<"Requested index for invalid parent, row="<<row;
              if ((row>=0) && (row<m_models.count()) && (column==0))
                return createIndex(row,column);
          }
          return QModelIndex();
        }

        QVariant CategorizedSnippetModel::data(const QModelIndex &index, int role) const {
          if (!index.isValid())
          {
            
//             kDebug()<<"invoked with invalid index";
            return QVariant();
          }
          if (!index.internalPointer()) {
//             kDebug()<<"invoked for entry with no internalPointer: row:"<<index.row()<<endl;
            if ( (role==Qt::DisplayRole) || (role==SnippetSelectorModel::FileTypeRole) )
              return m_models[index.row()]->fileType();
            if (role==SnippetSelectorModel::MergedFilesRole) {
                return m_models[index.row()]->data(QModelIndex(),SnippetSelectorModel::MergedFilesRole);
            }
          } else {
//             kDebug()<<"invoked for valid parent";
            SnippetSelectorModel *m=(SnippetSelectorModel*)(index.internalPointer());
            Q_ASSERT(m);
            if (role==SnippetSelectorModel::FileTypeRole)
              return m->fileType();
            return m->data(m->index(index.row(),index.column(),QModelIndex()),role);
          }
          return QVariant();
        }

        QModelIndex CategorizedSnippetModel::parent ( const QModelIndex & index) const {
//           kDebug()<<index<<" valid?"<<index.isValid()<<" internalPointer:"<<index.internalPointer();
          if (!index.isValid()) return QModelIndex();
          if (index.internalPointer())
          {
            QModelIndex idx=createIndex(m_models.indexOf((SnippetSelectorModel*)(index.internalPointer())),0);
//             kDebug()<<"==========>"<<idx;
            return idx;
          }
          return QModelIndex();
        }

        QVariant CategorizedSnippetModel::headerData ( int section, Qt::Orientation orientation, int role) const {
          if (orientation==Qt::Vertical) return QVariant();
          if (section!=0) return QVariant();
          if (role!=Qt::DisplayRole) return QVariant();
          return QString("Snippet");
        }
        
        KActionCollection *CategorizedSnippetModel::actionCollection() {
          return m_actionCollection;
        }


//END: CategorizedSnippetModel


//BEGIN: InternalCompletionModel
        InternalCompletionModel::InternalCompletionModel(QList<FilteredEntry> _entries):
          CodeCompletionModel2(0),entries(_entries){}
        InternalCompletionModel::~InternalCompletionModel(){}
        void InternalCompletionModel::completionInvoked(KTextEditor::View* view, const KTextEditor::Range& range, InvocationType invocationType) {          
        }
        
        QVariant InternalCompletionModel::data( const QModelIndex & index, int role) const {
          //kDebug(13040);
          if (role == KTextEditor::CodeCompletionModel::InheritanceDepth)
            return 1;
          if (!index.parent().isValid()) {
            if (role==Qt::DisplayRole) return i18n("Snippets");
            if (role==GroupRole) return Qt::DisplayRole;
            return QVariant();
          }
          if (role == Qt::DisplayRole) {
            if (index.column() == KTextEditor::CodeCompletionModel::Name ) {
              //kDebug(13040)<<"returning: "<<m_matches[index.row()]->match;
              return entries[index.row()].model->d->entries[entries[index.row()].id].match;
            } else if (index.column() == KTextEditor::CodeCompletionModel::Prefix ) {
              const QString& tmp=entries[index.row()].model->d->entries[entries[index.row()].id].prefix;
              if (!tmp.isEmpty()) return tmp;
            } else if (index.column() == KTextEditor::CodeCompletionModel::Postfix ) {
              const QString& tmp=entries[index.row()].model->d->entries[entries[index.row()].id].postfix;
              if (!tmp.isEmpty()) return tmp;
            } else if (index.column() == KTextEditor::CodeCompletionModel::Arguments ) {
              const QString& tmp=entries[index.row()].model->d->entries[entries[index.row()].id].arguments;
              if (!tmp.isEmpty()) return tmp;
            }
          }
          return QVariant();
        }

        QModelIndex InternalCompletionModel::parent(const QModelIndex& index) const {
          if (index.internalId())
            return createIndex(0, 0, 0);
          else
            return QModelIndex();
        }

        QModelIndex InternalCompletionModel::index(int row, int column, const QModelIndex& parent) const {
          if (!parent.isValid()) {
            if (row == 0)
              return createIndex(row, column, 0); //header  index
            else
              return QModelIndex();
          } else if (parent.parent().isValid()) //we only have header and children, no subheaders
            return QModelIndex();

          if (row < 0 || row >= entries.count() || column < 0 || column >= ColumnCount )
            return QModelIndex();

          return createIndex(row, column, 1); // normal item index
        }

        int InternalCompletionModel::rowCount (const QModelIndex & parent) const {
          if (!parent.isValid() && !entries.isEmpty())
            return 1; //one toplevel node (group header)
          else if(parent.parent().isValid())
            return 0; //we don't have sub children
          else
            return entries.count(); // only the children
        }

        void InternalCompletionModel::executeCompletionItem2(KTextEditor::Document* document,
          const KTextEditor::Range& word, const QModelIndex& index) const {
            KTextEditor::View *view=document->activeView();
            int row=index.row();
            SnippetCompletionModel *m=entries[row].model;
            QString fillin=m->d->entries[entries[row].id].fillin;
              int script=m->d->entries[entries[row].id].script;
              TemplateScript * ts=0;
              if (script!=-1)
                ts=m->d->scripts[script];
            
              //kDebug()<<"snippet content to insert is:"<<fillin;
              
              KTextEditor::TemplateInterface2 *ti2=qobject_cast<KTextEditor::TemplateInterface2*>(view);
              if (ti2) {

                ti2->insertTemplateText (view->cursorPosition(), fillin,QMap<QString,QString>(),ts);
              } else {
                KTextEditor::TemplateInterface *ti=qobject_cast<KTextEditor::TemplateInterface*>(view);
                if (ti)
                  ti->insertTemplateText (view->cursorPosition(), fillin,QMap<QString,QString>());
              }
              view->setFocus();
            const_cast<InternalCompletionModel*>(this)->deleteLater();
        }


        void InternalCompletionModel::aborted(View* view) {
          //kDebug();
          deleteLater();
        }

/*
         virtual bool shouldAbortCompletion(View* view, const Range &range, const QString &currentCompletion);
        virtual Range completionRange(View* view, const Cursor &position);
        virtual bool shouldStartCompletion(KTextEditor::View* view, const QString &insertedText, bool userInsertion, const KTextEditor::Cursor &position);
*/


//END:  InternalCompletionModel



#ifdef SNIPPET_EDITOR
  }
#endif
  }
}


// kate: space-indent on; indent-width 2; replace-tabs on;
