/***************************************************************************
                          tcl_parser.cpp  -  description
                             -------------------
    begin                : Apr 2 2003
    author               : 2003 Massimo Callegari
    email                : massimocallegari@yahoo.it
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
#include <QPixmap>

void KatePluginSymbolViewerView::parseTclSymbols(void)
{
  if (!win->activeView())
   return;

 QString currline, prevline;
 bool    prevComment = false;
 QString varStr("set ");
 QString procStr("proc");
 QString stripped;
 int i, j, args_par = 0, graph = 0;
 char block = 0, parse_func = 0;

 QTreeWidgetItem *node = NULL;
 QTreeWidgetItem *mcrNode = NULL, *clsNode = NULL;
 QTreeWidgetItem *lastMcrNode = NULL, *lastClsNode = NULL;

 QPixmap mcr( ( const char** ) macro_xpm );
 QPixmap cls( ( const char** ) class_xpm );

 if(treeMode)
  {
   clsNode = new QTreeWidgetItem(symbols, QStringList( i18n("Functions") ) );
   mcrNode = new QTreeWidgetItem(symbols, QStringList( i18n("Globals") ) );
   clsNode->setIcon(0, QIcon(cls));
   mcrNode->setIcon(0, QIcon(mcr));

   lastMcrNode = mcrNode;
   lastClsNode = clsNode;

   if (expanded_on)
      {
       symbols->expandItem(clsNode);
       symbols->expandItem(mcrNode);
      }
   symbols->setRootIsDecorated(1);
  }
 else
   symbols->setRootIsDecorated(0);

 KTextEditor::Document *kDoc = win->activeView()->document();

 //positions.resize(kDoc->numLines() + 3); // Maximum symbols number o.O
 //positions.fill(0);

 for (i = 0; i<kDoc->lines(); i++)
   {
    currline = kDoc->line(i);
    currline = currline.trimmed();
    bool comment = false;
    //kDebug(13000)<<currline;
    if(currline == "") continue;
    if(currline.at(0) == '#') comment = true;

    if(i > 0)
      {
       prevline = kDoc->line(i-1);
       if(prevline.endsWith("\\") && prevComment) comment = true;
      }
    prevComment = comment;

    if(!comment)
      {
       if(currline.startsWith(varStr) && block == 0)
         {
          if (macro_on == true) // not really a macro, but a variable
            {
             stripped = currline.right(currline.length() - 3);
             stripped = stripped.simplified();
             int fnd = stripped.indexOf(' ');
             //fnd = stripped.indexOf(';');
             if(fnd > 0) stripped = stripped.left(fnd);

             if (treeMode)
               {
                node = new QTreeWidgetItem(mcrNode, lastMcrNode);
                lastMcrNode = node;
               }
             else
                node = new QTreeWidgetItem(symbols);
             node->setText(0, stripped);
             node->setIcon(0, QIcon(mcr));
             node->setText(1, QString::number( i, 10));
             stripped = "";
            }//macro
          } // starts with "set"

       else if(currline.startsWith(procStr)) { parse_func = 1; }

       if (parse_func == 1)
             {
              for (j = 0; j < currline.length(); j++)
                 {
                  if (block == 1)
                    {
                     if(currline.at(j)=='{') graph++;
                     if(currline.at(j)=='}')
                       {
                        graph--;
                        if (graph == 0) { block = 0; parse_func = 0; continue; }
                       }
                    }
                  if (block == 0)
                    {
                     stripped += currline.at(j);
                     if(currline.at(j) == '{') args_par++;
                     if(currline.at(j) == '}')
                         {
                          args_par--;
                          if (args_par == 0)
                            {
                             //stripped = stripped.simplified();
                             if(func_on == true)
                               {
                                if (treeMode)
                                  {
                                   node = new QTreeWidgetItem(clsNode, lastClsNode);
                                   lastClsNode = node;
                                  }
                                else
                                   node = new QTreeWidgetItem(symbols);
                                node->setText(0, stripped);
                                node->setIcon(0, QIcon(cls));
                                node->setText(1, QString::number( i, 10));
                               }
                             stripped = "";
                             block = 1;
                            }
                         }
                    } // block = 0
                  } // for j loop
               }//func_on
      } // not a comment
    } //for i loop

 //positions.resize(symbols->itemIndex(node) + 1);
}

