
#include "xml_parser_parts.h"



bool XMLParser::isClosingTag(KTextEditor::Document *doc,KTextEditor::Range tag)
{
	KTextEditor::Cursor cur1=tag.start();
	KTextEditor::Cursor cur2=tag.end();
	QString s=doc->line(cur1.line());
	s=s.right(cur1.column());
	s=s.left(cur2.column());
	if(s.indexOf("/")==1)
		return true;
	return false;
}
 
bool XMLParser::isStartTag(KTextEditor::Document *doc,KTextEditor::Range tag)
{
	KTextEditor::Cursor cur1=tag.start();
	KTextEditor::Cursor cur2=tag.end();
	QString s=doc->line(cur1.line());
	s=s.right(cur1.column());
	s=s.left(cur2.column());
	if(s.indexOf("/")!=1 && s.lastIndexOf("/")!=s.length()-2)
		return true;
	return false;
}

KTextEditor::Range XMLParser::getNextTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to)
{
    KTextEditor::SearchInterface *sface= qobject_cast<KTextEditor::SearchInterface*>(doc);
    return   (sface->searchText(KTextEditor::Range(from, to), "<[/]?[A-Za-z0-9]+>", KTextEditor::Search::Regex)).at(0);
}

//nu merge si Regex si Backwards in acelasi timp
KTextEditor::Range XMLParser::getPreviousTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to)
{
    KTextEditor::SearchInterface *sface= qobject_cast<KTextEditor::SearchInterface*>(doc);
    return   (sface->searchText(KTextEditor::Range(from, to),
                                "<[/]?[A-Za-z0-9]+>",
                                 KTextEditor::Search::Regex
                                )
              ).at(0);
}

QString XMLParser::getTagName(KTextEditor::Range rng, KTextEditor::Document *doc)
{
    return doc->text(rng);
}

