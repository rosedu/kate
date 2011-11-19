/* This file is part of the KDE project
   Copyright (C) 2001 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2002 Joseph Wenninger <jowenn@kde.org>
   Copyright (C) 2002 Anders Lund <anders.lund@lund.tdcadsl.dk>

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

#ifndef __KATE_CONSOLE_H__
#define __KATE_CONSOLE_H__

#include <kate/plugin.h>
#include <kate/mainwindow.h>
#include <kate/pluginconfigpageinterface.h>
#include <kurl.h>

#include <kvbox.h>
#include <QList>

class QShowEvent;

namespace KParts
{
  class ReadOnlyPart;
}

namespace KateMDI
{
  }

class KateConsole;
class KateKonsolePluginView;

class KateKonsolePlugin: public Kate::Plugin, public Kate::PluginConfigPageInterface
{
    Q_OBJECT
    Q_INTERFACES(Kate::PluginConfigPageInterface)
    
  friend class KateKonsolePluginView;
  
  public:
    explicit KateKonsolePlugin( QObject* parent = 0, const QList<QVariant>& = QList<QVariant>() );
    virtual ~KateKonsolePlugin();

    Kate::PluginView *createView (Kate::MainWindow *mainWindow);

    // PluginConfigPageInterface
    uint configPages() const { return 1; };
    Kate::PluginConfigPage *configPage (uint number = 0, QWidget *parent = 0, const char *name = 0);
    QString configPageName (uint number = 0) const;
    QString configPageFullName (uint number = 0) const;
    KIcon configPageIcon (uint number = 0) const;

    void readConfig();

    QByteArray previousEditorEnv() {return m_previousEditorEnv;}
    
  private:
    QList<KateKonsolePluginView*> mViews;
    QByteArray m_previousEditorEnv;
};

class KateKonsolePluginView : public Kate::PluginView
{
    Q_OBJECT

  public:
    /**
      * Constructor.
      */
    KateKonsolePluginView (KateKonsolePlugin* plugin, Kate::MainWindow *mainWindow);

    /**
     * Virtual destructor.
     */
    ~KateKonsolePluginView ();

    void readConfig();

  private:
    KateKonsolePlugin *m_plugin;
    KateConsole *m_console;
};

/**
 * KateConsole
 * This class is used for the internal terminal emulator
 * It uses internally the konsole part, thx to konsole devs :)
 */
class KateConsole : public KVBox, public Kate::XMLGUIClient
{
    Q_OBJECT

  public:
    /**
     * construct us
     * @param mw main window
     * @param parent toolview
     */
    KateConsole (KateKonsolePlugin* plugin, Kate::MainWindow *mw, QWidget* parent);

    /**
     * destruct us
     */
    ~KateConsole ();

    void readConfig();

    /**
     * cd to dir
     * @param url given dir
     */
    void cd (const KUrl &url);

    /**
     * send given text to console
     * @param text commands for console
     */
    void sendInput( const QString& text );

    Kate::MainWindow *mainWindow()
    {
      return m_mw;
    }

  public Q_SLOTS:
    /**
     * pipe current document to console
     */
    void slotPipeToConsole ();

    /**
     * synchronize the konsole with the current document (cd to the directory)
     */
    void slotSync();
    /**
     * When syncing is done by the user, also show the terminal if it is hidden
     */
    void slotManualSync();

  private Q_SLOTS:
    /**
     * the konsole exited ;)
     * handle that, hide the dock
     */
    void slotDestroyed ();

    /**
     * construct console if needed
     */
    void loadConsoleIfNeeded();

    /**
     * set or clear focus as appropriate.
     */
    void slotToggleFocus();

  protected:
    /**
     * the konsole get shown
     * @param ev show event
     */
    void showEvent(QShowEvent *ev);

  private:
    /**
     * console part
     */
    KParts::ReadOnlyPart *m_part;

    /**
     * main window of this console
     */
    Kate::MainWindow *m_mw;

    /**
     * toolview for this console
     */
    QWidget *m_toolView;
    
    KateKonsolePlugin *m_plugin;
};

class KateKonsoleConfigPage : public Kate::PluginConfigPage {
    Q_OBJECT
  public:
    explicit KateKonsoleConfigPage( QWidget* parent = 0, KateKonsolePlugin *plugin = 0 );
    virtual ~KateKonsoleConfigPage()
    {}

    virtual void apply();
    virtual void reset();
    virtual void defaults()
    {}
  private:
    class QCheckBox *cbAutoSyncronize;
    class QCheckBox *cbSetEditor;
    KateKonsolePlugin *mPlugin;
};
#endif
// kate: space-indent on; indent-width 2; replace-tabs on;

