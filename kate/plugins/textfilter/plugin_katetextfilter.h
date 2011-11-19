 /***************************************************************************
                          plugin_katetextfilter.h  -  description
                             -------------------
    begin                : FRE Feb 23 2001
    copyright            : (C) 2001 by Joseph Wenninger
    email                : jowenn@bigfoot.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef PLUGIN_KATETEXTFILTER_H
#define PLUGIN_KATETEXTFILTER_H

#include <kate/plugin.h>
#include <kate/application.h>
#include <kate/documentmanager.h>
#include <kate/mainwindow.h>

#include <ktexteditor/view.h>
#include <ktexteditor/document.h>
#include <ktexteditor/commandinterface.h>

#include <QProcess>
#include <QVariantList>

class KProcess;

class PluginKateTextFilter : public Kate::Plugin, public KTextEditor::Command
{
  Q_OBJECT

  public:
    explicit PluginKateTextFilter( QObject* parent = 0, const QVariantList& = QVariantList() );
    virtual ~PluginKateTextFilter();

    Kate::PluginView *createView (Kate::MainWindow *mainWindow);

    // Kate::Command
    const QStringList& cmds ();
    bool exec (KTextEditor::View *view, const QString &cmd, QString &msg);
    bool help (KTextEditor::View *view, const QString &cmd, QString &msg);
  private:
    void runFilter( KTextEditor::View *kv, const QString & filter );

  private:
    QString  m_strFilterOutput;
    KProcess * m_pFilterProcess;
    QStringList completionList;
    bool pasteResult;
  public slots:
    void slotEditFilter ();
    void slotFilterReceivedStdout ();
    void slotFilterReceivedStderr ();
    void slotFilterProcessExited (int exitCode, QProcess::ExitStatus exitStatus);
};

class PluginViewKateTextFilter: public Kate::PluginView, public Kate::XMLGUIClient
{
  Q_OBJECT

  public:
    PluginViewKateTextFilter(PluginKateTextFilter *plugin, Kate::MainWindow *mainwindow);
    virtual ~PluginViewKateTextFilter();

  private:
    PluginKateTextFilter *m_plugin;
};

#endif // PLUGIN_KATETEXTFILTER_H

// kate: space-indent on; indent-width 2; replace-tabs on; mixed-indent off;
