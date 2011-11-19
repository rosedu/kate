/***************************************************************************
                          php_parser.cpp  -  description
                             -------------------
    begin         : Apr 1st 2007
    last update   : Sep 14th 2010
    author(s)     : 2007, Massimo Callegari <massimocallegari@yahoo.it>
                  : 2010, Emmanuel Bouthenot <kolter@openics.org>
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

void KatePluginSymbolViewerView::parsePhpSymbols(void)
{
  if (win->activeView())
  {
    QString line, lineWithliterals;
    QPixmap namespacePix( ( const char** ) class_int_xpm );
    QPixmap definePix( ( const char** ) macro_xpm );
    QPixmap varPix( ( const char** ) struct_xpm );
    QPixmap classPix( ( const char** ) class_xpm );
    QPixmap constPix( ( const char** ) macro_xpm );
    QPixmap functionPix( ( const char** ) method_xpm );
    QTreeWidgetItem *node = NULL;
    QTreeWidgetItem *namespaceNode = NULL, *defineNode = NULL, \
        *classNode = NULL, *functionNode = NULL;
    QTreeWidgetItem *lastNamespaceNode = NULL, *lastDefineNode = NULL, \
        *lastClassNode = NULL, *lastFunctionNode = NULL;

    KTextEditor::Document *kv = win->activeView()->document();

    if (treeMode)
    {
      namespaceNode = new QTreeWidgetItem(symbols, QStringList( i18n("Namespaces") ) );
      defineNode = new QTreeWidgetItem(symbols, QStringList( i18n("Defines") ) );
      classNode = new QTreeWidgetItem(symbols, QStringList( i18n("Classes") ) );
      functionNode = new QTreeWidgetItem(symbols, QStringList( i18n("Functions") ) );

      namespaceNode->setIcon(0, QIcon( namespacePix ) );
      defineNode->setIcon(0, QIcon( definePix ) );
      classNode->setIcon(0, QIcon( classPix ) );
      functionNode->setIcon(0, QIcon( functionPix ) );

      if (expanded_on)
      {
        symbols->expandItem(namespaceNode);
        symbols->expandItem(defineNode);
        symbols->expandItem(classNode);
        symbols->expandItem(functionNode);
      }

      lastNamespaceNode = namespaceNode;
      lastDefineNode = defineNode;
      lastClassNode = classNode;
      lastFunctionNode = functionNode;

      symbols->setRootIsDecorated(1);
    }
    else
    {
      symbols->setRootIsDecorated(0);
    }

    // Namespaces: http://www.php.net/manual/en/language.namespaces.php
    QRegExp namespaceRegExp("^namespace\\s+([^;\\s]+)", Qt::CaseInsensitive);
    // defines: http://www.php.net/manual/en/function.define.php
    QRegExp defineRegExp("(^|\\W)define\\s*\\(\\s*['\"]([^'\"]+)['\"]", Qt::CaseInsensitive);
    // classes: http://www.php.net/manual/en/language.oop5.php
    QRegExp classRegExp("^((abstract\\s+|final\\s+)?)class\\s+([\\w_][\\w\\d_]*)\\s*(implements\\s+[\\w\\d_]*)?", Qt::CaseInsensitive);
    // interfaces: http://www.php.net/manual/en/language.oop5.php
    QRegExp interfaceRegExp("^interface\\s+([\\w_][\\w\\d_]*)", Qt::CaseInsensitive);
    // classes constants: http://www.php.net/manual/en/language.oop5.constants.php
    QRegExp constantRegExp("^const\\s+([\\w_][\\w\\d_]*)", Qt::CaseInsensitive);
    // functions: http://www.php.net/manual/en/language.oop5.constants.php
    QRegExp functionRegExp("^((public|protected|private)?(\\s*static)?\\s+)?function\\s+&?\\s*([\\w_][\\w\\d_]*)\\s*(.*)$", Qt::CaseInsensitive);
    // variables: http://www.php.net/manual/en/language.oop5.properties.php
    QRegExp varRegExp("^((var|public|protected|private)?(\\s*static)?\\s+)?\\$([\\w_][\\w\\d_]*)", Qt::CaseInsensitive);

    // function args detection: “function a($b, $c=null)” => “$b, $v”
    QRegExp functionArgsRegExp("(\\$[\\w_]+)", Qt::CaseInsensitive);
    QStringList functionArgsList;
    QString functionArgs;

    // replace literals by empty strings: “function a($b='nothing', $c="pretty \"cool\" string")” => “function ($b='', $c="")”
    QRegExp literalRegExp("([\"'])(?:\\\\.|[^\\\\])*\\1");
    literalRegExp.setMinimal(true);
    // remove useless comments: “public/* static */ function a($b, $c=null) /* test */” => “public function a($b, $c=null)”
    QRegExp blockCommentInline("/\\*.*\\*/");
    blockCommentInline.setMinimal(true);

    int i, pos;
    bool isClass, isInterface;
    bool inBlockComment = false;
    bool inClass = false, inFunction = false;

    //QString debugBuffer("SymbolViewer(PHP), line %1 %2 → [%3]");

    for (i=0; i<kv->lines(); i++)
    {
      //kdDebug(13000) << debugBuffer.arg(i, 4).arg("=origin", 10).arg(kv->line(i));

      line = kv->line(i).simplified();
      //kdDebug(13000) << debugBuffer.arg(i, 4).arg("+simplified", 10).arg(line);

      // keeping a copy with literals for catching “defines()”
      lineWithliterals = line;

      // reduce literals to empty strings to not match comments separators in literals
      line.replace(literalRegExp, "\\1\\1");
      //kdDebug(13000) << debugBuffer.arg(i, 4).arg("-literals", 10).arg(line);

      line.replace(blockCommentInline, "");
      //kdDebug(13000) << debugBuffer.arg(i, 4).arg("-comments", 10).arg(line);

      // trying to find comments and to remove commented parts
      pos = line.indexOf("#");
      if (pos >= 0)
      {
          line = line.left(pos);
      }
      pos = line.indexOf("//");
      if (pos >= 0)
      {
          line = line.left(pos);
      }
      pos = line.indexOf("/*");
      if (pos >= 0)
      {
          line = line.left(pos);
          inBlockComment = true;
      }
      pos = line.indexOf("*/");
      if (pos >= 0)
      {
          line = line.right(line.length() - pos - 2);
          inBlockComment = false;
      }

      if (inBlockComment)
      {
          continue;
      }

      // trimming again after having removed the comments
      line = line.simplified();
      //kdDebug(13000) << debugBuffer.arg(i, 4).arg("+simplified", 10).arg(line);

      // detect NameSpaces
      if (namespaceRegExp.indexIn(line) != -1)
      {
        if (treeMode)
        {
          node = new QTreeWidgetItem(namespaceNode, lastNamespaceNode);
          if (expanded_on)
          {
            symbols->expandItem(node);
          }
          lastNamespaceNode = node;
        }
        else
        {
          node = new QTreeWidgetItem(symbols);
        }
        node->setText(0, namespaceRegExp.cap(1));
        node->setIcon(0, QIcon(namespacePix));
        node->setText(1, QString::number( i, 10));
      }

      // detect defines
      if (defineRegExp.indexIn(lineWithliterals) != -1)
      {
          if (treeMode)
          {
            node = new QTreeWidgetItem(defineNode, lastDefineNode);
            lastDefineNode = node;
          }
          else
          {
            node = new QTreeWidgetItem(symbols);
          }
          node->setText(0, defineRegExp.cap(2));
          node->setIcon(0, QIcon(definePix));
          node->setText(1, QString::number( i, 10));
      }

      // detect classes, interfaces
      isClass = classRegExp.indexIn(line) != -1;
      isInterface = interfaceRegExp.indexIn(line) != -1;
      if (isClass || isInterface)
      {
        if (treeMode)
        {
          node = new QTreeWidgetItem(classNode, lastClassNode);
          if (expanded_on)
          {
            symbols->expandItem(node);
          }
          lastClassNode = node;
        }
        else
        {
          node = new QTreeWidgetItem(symbols);
        }
        if (isClass)
        {
          if (types_on && !classRegExp.cap(1).trimmed().isEmpty() && !classRegExp.cap(4).trimmed().isEmpty())
          {
            node->setText(0, classRegExp.cap(3)+" ["+classRegExp.cap(1).trimmed()+","+classRegExp.cap(4).trimmed()+"]");
          }
          else if (types_on && !classRegExp.cap(1).trimmed().isEmpty())
          {
            node->setText(0, classRegExp.cap(3)+" ["+classRegExp.cap(1).trimmed()+"]");
          }
          else if (types_on && !classRegExp.cap(4).trimmed().isEmpty())
          {
            node->setText(0, classRegExp.cap(3)+" ["+classRegExp.cap(4).trimmed()+"]");
          }
          else
          {
            node->setText(0, classRegExp.cap(3));
          }
        }
        else
        {
          if (types_on)
          {
            node->setText(0, interfaceRegExp.cap(1) + " [interface]");
          }
          else
          {
            node->setText(0, interfaceRegExp.cap(1));
          }
        }
        node->setIcon(0, QIcon(classPix));
        node->setText(1, QString::number( i, 10));
        inClass = true;
        inFunction = false;
      }

      // detect class constants
      if (constantRegExp.indexIn(line) != -1)
      {
        if (treeMode)
        {
          node = new QTreeWidgetItem(lastClassNode);
        }
        else
        {
          node = new QTreeWidgetItem(symbols);
        }
        node->setText(0, constantRegExp.cap(1));
        node->setIcon(0, QIcon(constPix));
        node->setText(1, QString::number( i, 10));
      }

      // detect class variables
      if (inClass && !inFunction)
      {
        if (varRegExp.indexIn(line) != -1)
        {
          if (treeMode && inClass)
          {
            node = new QTreeWidgetItem(lastClassNode);
          }
          else
          {
            node = new QTreeWidgetItem(symbols);
          }
          node->setText(0, varRegExp.cap(4));
          node->setIcon(0, QIcon(varPix));
          node->setText(1, QString::number( i, 10));
        }
      }

      // detect functions
      if (functionRegExp.indexIn(line) != -1)
      {
        if (treeMode && inClass)
        {
          node = new QTreeWidgetItem(lastClassNode);
        }
        else if (treeMode)
        {
          node = new QTreeWidgetItem(lastFunctionNode);
        }
        else
        {
          node = new QTreeWidgetItem(symbols);
        }

        if (types_on)
        {
          QString functionArgs(functionRegExp.cap(5));
          pos = 0;
          while (pos >= 0) {
            pos = functionArgsRegExp.indexIn(functionArgs, pos);
            if (pos >= 0) {
              pos += functionArgsRegExp.matchedLength();
              functionArgsList += functionArgsRegExp.cap(1);
            }
          }
          node->setText(0, functionRegExp.cap(4) + "(" + functionArgsList.join(", ") + ")");
          functionArgsList.clear();
        }
        else
        {
          node->setText(0, functionRegExp.cap(4));
        }
        node->setIcon(0, QIcon(functionPix));
        node->setText(1, QString::number( i, 10));

        inFunction = true;
      }
    }
  }
}

