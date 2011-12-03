#ifndef _PARSER_LIB_
#define _PARSER_LIB_
#include <KTextEditor/Range>
#include <KTextEditor/Cursor>
#include <KTextEditor/Document>
#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/markinterface.h>
#include <ktexteditor/movingrange.h>
#include <ktexteditor/movinginterface.h>
#include<ktexteditor/searchinterface.h>
#include<QString>
class XMLParser:KTextEditor::Document
{
public:
    static  KTextEditor::Range getNextTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to);
    static  KTextEditor::Range getPreviousTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to);
    static  QString getTagName(KTextEditor::Range rng, KTextEditor::Document *doc);
    static bool isClosingTag(KTextEditor::Document *doc,KTextEditor::Range tag);
    static bool isStartTag(KTextEditor::Document *doc,KTextEditor::Range tag);
    static QString getTagName(KTextEditor::Document *doc, KTextEditor::Range rng);
    static KTextEditor::Range insideTag(KTextEditor::Document *doc, KTextEditor::Cursor crs);
    static KTextEditor::Range findMatch(KTextEditor::Document *doc, KTextEditor::Range searchedRange);
};
#endif
