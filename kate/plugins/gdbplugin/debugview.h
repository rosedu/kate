//
// debugview.h
//
// Description: Manages the interaction with GDB
//
//
// Copyright (c) 2008-2010 Ian Wakeling <ian.wakeling@ntlworld.com>
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

#ifndef DEBUGVIEW_H
#define DEBUGVIEW_H

#include <QtCore/QObject>

#include <kprocess.h>
#include <kurl.h>

class DebugView : public QObject
{
Q_OBJECT
public:
    DebugView( QObject* parent );
    ~DebugView();

    void runDebugger(   QString const&  workingDirectory,
                        QString const&  target,
                        QString const&  arguments );
    bool debuggerRunning() const;
    bool debuggerBusy() const;
    bool hasBreakpoint( KUrl const& url, int line );

    void toggleBreakpoint( KUrl const& url, int line );
    void movePC( KUrl const& url, int line );
    void runToCursor( KUrl const& url, int line );

    void issueCommand( QString const& cmd );


public Q_SLOTS:
    void slotInterrupt();
    void slotStepInto();
    void slotStepOver();
    void slotStepOut();
    void slotContinue();
    void slotKill();
    void slotReRun();

private Q_SLOTS:
    void slotError();
    void slotReadDebugStdOut();
    void slotReadDebugStdErr();
    void slotDebugFinished( int exitCode, QProcess::ExitStatus status );
    void issueNextCommand();

Q_SIGNALS:
    void debugLocationChanged( const KUrl &file, int lineNum );
    void breakPointSet( const KUrl &file, int lineNum );
    void breakPointCleared( const KUrl &file, int lineNum );
    void clearBreakpointMarks();
    void stackFrameInfo( QString const& level, QString const& info );
    void stackFrameChanged( int level );
    void threadInfo( int number, bool avtive );

    void infoLocal( QString const& line );

    void outputText( const QString &text );
    void outputError( const QString &text );
    void readyForInput( bool ready );
    void programEnded();
    void gdbEnded();

private:
    enum State
    {
        none,
        ready,
        executingCmd,
        listingBreakpoints,
        infoStack,
        infoArgs,
        infoLocals,
        infoThreads
    };

    enum SubState
    {
        normal,
        stackFrameSeen,
        stackTraceSeen
    };

    struct BreakPoint
    {
        int  number;
        KUrl file;
        int  line;
    };

private:
    void processLine( QString output );
    void processErrors();
    void outputTextMaybe( const QString &text );
    KUrl resolveFileName( const QString &fileName );

private:
    KProcess            m_debugProcess;
    QString             m_workingDirectory;
    QString             m_target;
    QString             m_arguments;
    State               m_state;
    SubState            m_subState;
    QString             m_currentFile;
    QString             m_newFrameFile;
    int                 m_newFrameLevel;
    QStringList         m_nextCommands;
    QString             m_lastCommand;
    bool                m_debugLocationChanged;
    QList<BreakPoint>   m_breakPointList;
    QString             m_outBuffer;
    QString             m_errBuffer;
    QStringList         m_errorList;
};

#endif
