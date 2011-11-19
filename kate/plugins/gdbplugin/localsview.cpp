//
// Description: Widget that local variables of the gdb inferior
//
// Copyright (c) 2010 Kåre Särs <kare.sars@iki.fi>
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Library General Public
//  License version 2 as published by the Free Software Foundation.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Library General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public License
//  along with this library; see the file COPYING.LIB.  If not, write to
//  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//  Boston, MA 02110-1301, USA.

#include "localsview.h"
#include <klocale.h>
#include <kdebug.h>


LocalsView::LocalsView(QWidget *parent)
:   QTreeWidget(parent), m_allAdded(true)
{
    QStringList headers;
    headers << i18n("Symbol");
    headers << i18n("Value");
    setHeaderLabels(headers);
    setAutoScroll(false);
}

LocalsView::~LocalsView()
{
}

void LocalsView::addLocal(const QString &vString)
{
    static QRegExp isValue("(\\S*)\\s=\\s(.*)");
    static QRegExp isStruct("\\{\\S*\\s=\\s.*");
    static QRegExp isStartPartial("\\S*\\s=\\s\\S*\\s=\\s\\{");
    static QRegExp isPrettyQList("\\s*\\[\\S*\\]\\s=\\s\\S*");
    static QRegExp isPrettyValue("(\\S*)\\s=\\s(\\S*)\\s=\\s(.*)");
    
    if (m_allAdded) {
        clear();
        m_allAdded = false;
    }
    
    if (vString.isEmpty()) {
        m_allAdded = true;
        resizeColumnToContents(1);
        return;
    }
    if (isStartPartial.exactMatch(vString)) {
        m_local = vString;
        return;
    }
    if (isPrettyQList.exactMatch(vString)) {
        m_local += vString.trimmed();
        if (m_local.endsWith(',')) m_local += ' ';
        return;
    }
    if (vString == "}") {
        m_local += vString;
    }

    QStringList symbolAndValue;
    QString value;
    
    if (m_local.isEmpty()) {
        if (vString == "No symbol table info available.") {
            return; /* this is not an error */
        }
        if (!isValue.exactMatch(vString)) {
            kDebug() << "Could not parse:" << vString;
            return;
        }
        symbolAndValue << isValue.cap(1);
        value = isValue.cap(2);
    }
    else {
        if (!isPrettyValue.exactMatch(m_local)) {
            kDebug() << "Could not parse:" << m_local;
            m_local.clear();
            return;
        }
        symbolAndValue << isPrettyValue.cap(1) << isPrettyValue.cap(2);
        value = isPrettyValue.cap(3);
    }

    QTreeWidgetItem *item;
    if (value[0] == '{') {
        if (value[1] == '{') {
            item = new QTreeWidgetItem(this, symbolAndValue);
            addArray(item, value.mid(1, value.size()-2));
        }
        else {
            if (isStruct.exactMatch(value)) {
                item = new QTreeWidgetItem(this, symbolAndValue);
                addStruct(item, value.mid(1, value.size()-2));
            }
            else {
                symbolAndValue << value;
                new QTreeWidgetItem(this, symbolAndValue);
            }
        }
    }
    else {
        symbolAndValue << value;
        new QTreeWidgetItem(this, symbolAndValue);
    }
    
    m_local.clear();
}


void LocalsView::addStruct(QTreeWidgetItem *parent, const QString &vString)
{
    static QRegExp isArray("\\{\\.*\\s=\\s.*");
    static QRegExp isStruct("\\.*\\s=\\s.*");
    QTreeWidgetItem *item;
    QStringList symbolAndValue;
    QString subValue;
    int start = 0;
    int end;
    while (start < vString.size()) {
        // Symbol
        symbolAndValue.clear();
        end = vString.indexOf(" = ", start);
        if (end < 0) {
            // error situation -> bail out
            symbolAndValue << vString.right(start);
            new QTreeWidgetItem(parent, symbolAndValue);
            break;
        }
        symbolAndValue << vString.mid(start, end-start);
        //kDebug() << symbolAndValue;
        // Value
        start = end + 3;
        end = start;
        if (vString[start] == '{') {
            start++;
            end++;
            int count = 1;
            bool inComment = false;
            // search for the matching }
            while(end < vString.size()) {
                if (!inComment) {
                    if (vString[end] == '"') inComment = true;
                    else if (vString[end] == '}') count--;
                    else if (vString[end] == '{') count++;
                    if (count == 0) break;
                }
                else {
                    if ((vString[end] == '"') && (vString[end-1] != '\\')) {
                        inComment = false;
                    }
                }
                end++;
            }
            subValue = vString.mid(start, end-start);
            if (isArray.exactMatch(subValue)) {
                item = new QTreeWidgetItem(parent, symbolAndValue);
                addArray(item, subValue);
            }
            else if (isStruct.exactMatch(subValue)) {
                item = new QTreeWidgetItem(parent, symbolAndValue);
                addStruct(item, subValue);
            }
            else {
                symbolAndValue << vString.mid(start, end-start);
                new QTreeWidgetItem(parent, symbolAndValue);
            }
            start = end + 3; // },_
        }
        else {
            // look for the end of the value in the vString
            bool inComment = false;
            while(end < vString.size()) {
                if (!inComment) {
                    if (vString[end] == '"') inComment = true;
                    else if (vString[end] == ',') break;
                }
                else {
                    if ((vString[end] == '"') && (vString[end-1] != '\\')) {
                        inComment = false;
                    }
                }
                end++;
            }
            symbolAndValue << vString.mid(start, end-start);
            new QTreeWidgetItem(parent, symbolAndValue);
            start = end + 2; // ,_
        }
    }
}

void LocalsView::addArray(QTreeWidgetItem *parent, const QString &vString)
{
    // getting here we have this kind of string:
    // "{...}" or "{...}, {...}" or ...
    QTreeWidgetItem *item;
    int count = 1;
    bool inComment = false;
    int index = 0;
    int start = 1;
    int end = 1;

    while (end < vString.size()) {
        if (!inComment) {
            if (vString[end] == '"') inComment = true;
            else if (vString[end] == '}') count--;
            else if (vString[end] == '{') count++;
            if (count == 0) {
                QStringList name;
                name << QString("[%1]").arg(index);
                index++;
                item = new QTreeWidgetItem(parent, name);
                addStruct(item, vString.mid(start, end-start));
                end += 4; // "}, {"
                start = end;
                count = 1;
            }
        }
        else {
            if ((vString[end] == '"') && (vString[end-1] != '\\')) {
                inComment = false;
            }
        }
        end++;
    }
}

