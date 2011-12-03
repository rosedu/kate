#include "xml_parser_parts.h"
/**
  return KTextEditor::Range(-1, -1, -1, -1) if the regex is not found
*/

//mai trebuie: shrink, verificare tag gol


KTextEditor::Range XMLParser::getNextTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to)
{
    KTextEditor::SearchInterface *sface= qobject_cast<KTextEditor::SearchInterface*>(doc);
    return (sface->searchText(KTextEditor::Range(from, to),
                              "<[/]?[A-Za-z0-9]+>",
                              KTextEditor::Search::Regex)
            ).at(0);
}

KTextEditor::Range XMLParser::getPreviousTag(KTextEditor::Document *doc, KTextEditor::Cursor from, KTextEditor::Cursor to)
{
    KTextEditor::SearchInterface *sface= qobject_cast<KTextEditor::SearchInterface*>(doc);
    return   (sface->searchText(KTextEditor::Range(from, to),
                                "<[/]?[A-Za-z0-9]+>",
                                 KTextEditor::Search::Regex | KTextEditor::Search::Backwards)
              ).at(0);
}

KTextEditor::Range XMLParser::insideTag(KTextEditor::Document *doc, KTextEditor::Cursor crs)
{
    KTextEditor::Cursor cBegin, cEnd;

    if(! getPreviousTag(doc, KTextEditor::Cursor(0,0), crs).isEmpty())
        cBegin = getPreviousTag(doc, KTextEditor::Cursor(0,0), crs).end();
    else cBegin = KTextEditor::Cursor(0,0);

    if(!getNextTag(doc, crs, doc->documentEnd()).isEmpty())
        cEnd = getNextTag(doc, crs, doc->documentEnd()).start();
    else cEnd = doc->documentEnd();

    KTextEditor::SearchInterface *sface = qobject_cast<KTextEditor::SearchInterface*>(doc);
    return sface->searchText(KTextEditor::Range(cBegin,cEnd), "<[/]?[A-Za-z0-9]+>", KTextEditor::Search::Regex).at(0);
}

QString XMLParser::getTagName(KTextEditor::Document *doc, KTextEditor::Range rng)
{
    if(rng == KTextEditor::Range(-1, -1, -1, -1)) return "";
    if(isStartTag(doc, rng))
    {
        if(doc->text(rng).indexOf(' ') == -1) return doc->text(rng).mid(1, doc->text(rng).size()-2);
        return doc->text(rng).mid(1, doc->text(rng).indexOf(' ')-2);
    }
    else
    {
        if(doc->text(rng).indexOf(' ') == -1) return doc->text(rng).mid(2, doc->text(rng).size()-3);
        return doc->text(rng).mid(2, doc->text(rng).indexOf(' ')-2);
    }
}

KTextEditor::Range XMLParser::findMatch(KTextEditor::Document *doc, KTextEditor::Range searchedRange)
{
    //if the match is found then it will be found here
    KTextEditor::Range rng01;

    //word we need to be found
    QString match = getTagName(doc, searchedRange);

    if(isStartTag(doc, searchedRange))
    {
        rng01 = getNextTag(doc, searchedRange.end(), doc->documentEnd());
        while(getTagName(doc, rng01).compare(match) != 0 && rng01 != KTextEditor::Range(-1, -1, -1, -1))
                rng01 = getNextTag(doc,rng01.end(), doc->documentEnd());
    }
    else{
        rng01 = getPreviousTag(doc, KTextEditor::Cursor(0, 0), searchedRange.start());
        while(getTagName(doc, rng01).compare(match) != 0 && rng01 != KTextEditor::Range(-1, -1, -1, -1))
            rng01 = getPreviousTag(doc, KTextEditor::Cursor(0, 0), rng01.start());
    }
    if(doc->documentRange().contains(rng01))
        return rng01;
    else return KTextEditor::Range(-1, -1, -1, -1);
}

bool XMLParser::isClosingTag(KTextEditor::Document *doc,KTextEditor::Range tag)
{
    if(KTextEditor::Range(-1, -1, -1, -1) == tag)
        return false;//optional
    return (doc->text(tag).at(1) == '/');
}

bool XMLParser::isStartTag(KTextEditor::Document *doc,KTextEditor::Range tag)
{
        if(KTextEditor::Range(-1, -1, -1, -1) == tag)
            return false;//optional
        return (doc->text(tag).at(1) != '/' && doc->text(tag).at(doc->text(tag).size()-2) !='/');
}

