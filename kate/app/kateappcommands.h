/* This file is part of the KDE project
   Copyright (C) 2009 Erlend Hamberg <ehamberg@gmail.com>
   Copyright (C) 2011 Svyatoslav Kuzmich <svatoslav1@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KATE_APP_COMMANDS_INCLUDED
#define KATE_APP_COMMANDS_INCLUDED

#include <ktexteditor/commandinterface.h>
#include <ktexteditor/view.h>

class KateAppCommands : public KTextEditor::Command
{
public:
    KateAppCommands();
    virtual ~KateAppCommands();
    virtual const QStringList &cmds ();
    virtual bool exec (KTextEditor::View *view, const QString &cmd, QString &msg);
    virtual bool help (KTextEditor::View *view, const QString &cmd, QString &msg);

private:
    QRegExp re_write;
    QRegExp re_quit;
    QRegExp re_exit;
    QRegExp re_edit;
    QRegExp re_new;
    QRegExp re_split;
    QRegExp re_vsplit;
    QRegExp re_bufferNext;
    QRegExp re_bufferPrev;
    QRegExp re_bufferFirst;
    QRegExp re_bufferLast;
    QRegExp re_editBuffer;

};

#endif
