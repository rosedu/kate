/***************************************************************************
                          xslt_parser.cpp  -  description
                             -------------------
    begin                : Mar 28 2007
    author               : 2007 jiri Tyr
    email                : jiri.tyr@vslib.cz
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

void KatePluginSymbolViewerView::parseXsltSymbols(void)
{
  if (!win->activeView())
   return;

 popup->changeItem( popup->idAt(2),i18n("Show Params"));
 popup->changeItem( popup->idAt(3),i18n("Show Variables"));
 popup->changeItem( popup->idAt(4),i18n("Show Templates"));

 QString cl; // Current Line
 QString stripped;

 char comment = 0;
 char templ = 0;
 int i;

 QPixmap cls( ( const char** ) class_xpm );
 QPixmap sct( ( const char** ) struct_xpm );
 QPixmap mcr( ( const char** ) macro_xpm );
 QPixmap cls_int( ( const char** ) class_int_xpm );

 QTreeWidgetItem *node = NULL;
 QTreeWidgetItem *mcrNode = NULL, *sctNode = NULL, *clsNode = NULL;
 QTreeWidgetItem *lastMcrNode = NULL, *lastSctNode = NULL, *lastClsNode = NULL;

 KTextEditor::Document *kv = win->activeView()->document();
 //kdDebug(13000)<<"Lines counted :"<<kv->numLines()<<endl;


 if(treeMode)
   {
    mcrNode = new QTreeWidgetItem(symbols, QStringList( i18n("Params") ) );
    sctNode = new QTreeWidgetItem(symbols, QStringList( i18n("Variables") ) );
    clsNode = new QTreeWidgetItem(symbols, QStringList( i18n("Templates") ) );
    mcrNode->setIcon(0, QIcon(mcr));
    sctNode->setIcon(0, QIcon(sct));
    clsNode->setIcon(0, QIcon(cls));

    if (expanded_on) 
      {
       symbols->expandItem(mcrNode);
       symbols->expandItem(sctNode);
       symbols->expandItem(clsNode);
      }

    lastMcrNode = mcrNode;
    lastSctNode = sctNode;
    lastClsNode = clsNode;

    symbols->setRootIsDecorated(1);
   }
 else
   {
    symbols->setRootIsDecorated(0);
   }

 for (i=0; i<kv->lines(); i++)
    {
     cl = kv->line(i);
     cl = cl.trimmed();

     if(cl.indexOf(QRegExp("<!--")) >= 0) { comment = 1; }
     if(cl.indexOf(QRegExp("-->")) >= 0) { comment = 0; continue; }

     if(cl.indexOf(QRegExp("^</xsl:template>")) >= 0) { templ = 0; continue; }

     if (comment==1) { continue; }
     if (templ==1) { continue; }

     if(cl.indexOf(QRegExp("^<xsl:param ")) == 0 && macro_on)
       {
        QString stripped = cl.remove(QRegExp("^<xsl:param +name=\""));
        stripped = stripped.remove(QRegExp("\".*"));

        if (treeMode)
          {
           node = new QTreeWidgetItem(mcrNode, lastMcrNode);
           lastMcrNode = node;
          }
        else node = new QTreeWidgetItem(symbols);
        node->setText(0, stripped);
        node->setIcon(0, QIcon(mcr));
        node->setText(1, QString::number( i, 10));
       }

     if(cl.indexOf(QRegExp("^<xsl:variable ")) == 0 && struct_on)
       {
        QString stripped = cl.remove(QRegExp("^<xsl:variable +name=\""));
        stripped = stripped.remove(QRegExp("\".*"));

        if (treeMode)
          {
           node = new QTreeWidgetItem(sctNode, lastSctNode);
           lastSctNode = node;
          }
        else node = new QTreeWidgetItem(symbols);
        node->setText(0, stripped);
        node->setIcon(0, QIcon(sct));
        node->setText(1, QString::number( i, 10));
       }

     if(cl.indexOf(QRegExp("^<xsl:template +match=")) == 0 && func_on)
       {
        QString stripped = cl.remove(QRegExp("^<xsl:template +match=\""));
        stripped = stripped.remove(QRegExp("\".*"));

        if (treeMode)
          {
           node = new QTreeWidgetItem(clsNode, lastClsNode);
           lastClsNode = node;
          }
        else node = new QTreeWidgetItem(symbols);
        node->setText(0, stripped);
        node->setIcon(0, QIcon(cls_int));
        node->setText(1, QString::number( i, 10));
       }

     if(cl.indexOf(QRegExp("^<xsl:template +name=")) == 0 && func_on)
       {
        QString stripped = cl.remove(QRegExp("^<xsl:template +name=\""));
        stripped = stripped.remove(QRegExp("\".*"));

        if (treeMode)
          {
           node = new QTreeWidgetItem(clsNode, lastClsNode);
           lastClsNode = node;
          }
        else node = new QTreeWidgetItem(symbols);
        node->setText(0, stripped);
        node->setIcon(0, QIcon(cls));
        node->setText(1, QString::number( i, 10));

       }

     if(cl.indexOf(QRegExp("<xsl:template")) >= 0) 
       {
        templ = 1;
       }
    }
}
