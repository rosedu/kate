/* Description : Kate CTags plugin
 * 
 * Copyright (C) 2008-2011 by Kare Sars <kare.sars@iki.fi>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kate_ctags_plugin.h"

#include <QFileInfo>
#include <KFileDialog>
#include <QCheckBox>

#include <kmenu.h>
#include <kactioncollection.h>
#include <kstringhandler.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>

#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kaboutdata.h>

K_PLUGIN_FACTORY(KateCTagsPluginFactory, registerPlugin<KateCTagsPlugin>();)
K_EXPORT_PLUGIN(KateCTagsPluginFactory(KAboutData("katectags", "kate-ctags-plugin",
                                                  ki18n("CTags Plugin"), "0.2",
                                                  ki18n( "CTags Plugin"))))

/******************************************************************/
KateCTagsPlugin::KateCTagsPlugin(QObject* parent, const QList<QVariant>&):
Kate::Plugin ((Kate::Application*)parent), m_view(0)
{
    KGlobal::locale()->insertCatalog("kate-ctags-plugin");
}

/******************************************************************/
Kate::PluginView *KateCTagsPlugin::createView(Kate::MainWindow *mainWindow)
{
    m_view = new KateCTagsView(mainWindow, KateCTagsPluginFactory::componentData());
    return m_view;
}


/******************************************************************/
Kate::PluginConfigPage *KateCTagsPlugin::configPage (uint number, QWidget *parent, const char *)
{
  if (number != 0) return 0;
  return new KateCTagsConfigPage(parent, this);
}

/******************************************************************/
QString KateCTagsPlugin::configPageName (uint number) const
{
    if (number != 0) return QString();
    return i18n("CTags");
}

/******************************************************************/
QString KateCTagsPlugin::configPageFullName (uint number) const
{
    if (number != 0) return QString();
    return i18n("CTags Settings");
}

/******************************************************************/
KIcon KateCTagsPlugin::configPageIcon (uint number) const
{
    if (number != 0) return KIcon();
    return KIcon("text-x-csrc");
}

/******************************************************************/
void KateCTagsPlugin::readConfig()
{
}




/******************************************************************/
KateCTagsConfigPage::KateCTagsConfigPage( QWidget* parent, KateCTagsPlugin *plugin )
: Kate::PluginConfigPage( parent )
, m_plugin( plugin )
{
    m_confUi.setupUi(this);
    m_confUi.cmdEdit->setText(DEFAULT_CTAGS_CMD);

    m_confUi.addButton->setToolTip(i18n("Add a directory to index."));
    m_confUi.addButton->setIcon(KIcon("list-add"));

    m_confUi.delButton->setToolTip(i18n("Remove a directory."));
    m_confUi.delButton->setIcon(KIcon("list-remove"));

    m_confUi.updateDB->setToolTip(i18n("(Re-)generate the common CTags database."));
    m_confUi.updateDB->setIcon(KIcon("view-refresh"));

    connect(m_confUi.updateDB,  SIGNAL(clicked()), this, SLOT(updateGlobalDB()));
    connect(m_confUi.addButton, SIGNAL(clicked()), this, SLOT(addGlobalTagTarget()));
    connect(m_confUi.delButton, SIGNAL(clicked()), this, SLOT(delGlobalTagTarget()));

    connect(&m_proc, SIGNAL(finished(int,QProcess::ExitStatus)), 
            this,    SLOT(updateDone(int,QProcess::ExitStatus)));
    
    reset();
}

/******************************************************************/
void KateCTagsConfigPage::apply()
{
    KConfigGroup config(KGlobal::config(), "CTags");
    config.writeEntry("GlobalCommand", m_confUi.cmdEdit->text());

    config.writeEntry("GlobalNumTargets", m_confUi.targetList->count());
    
    QString nr;
    for (int i=0; i<m_confUi.targetList->count(); i++) {
        nr = QString("%1").arg(i,3);
        config.writeEntry("GlobalTarget_"+nr, m_confUi.targetList->item(i)->text());
    }
    config.sync();
}

/******************************************************************/
void KateCTagsConfigPage::reset()
{
    KConfigGroup config(KGlobal::config(), "CTags");
    m_confUi.cmdEdit->setText(config.readEntry("GlobalCommand", DEFAULT_CTAGS_CMD));

    int numEntries = config.readEntry("GlobalNumTargets", 0);
    QString nr;
    QString target;
    for (int i=0; i<numEntries; i++) {
        nr = QString("%1").arg(i,3);
        target = config.readEntry("GlobalTarget_"+nr, QString());
        if (!listContains(target)) {
            new QListWidgetItem(target, m_confUi.targetList);
        }
    }
    config.sync();
}


/******************************************************************/
void KateCTagsConfigPage::addGlobalTagTarget()
{
    KFileDialog dialog(KUrl(), QString(), 0, 0);
    dialog.setMode(KFile::Directory | KFile::Files | KFile::ExistingOnly | KFile::LocalOnly);

    // i18n("CTags Database Location"));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList urls = dialog.selectedFiles();

    for (int i=0; i<urls.size(); i++) {
        if (!listContains(urls[i])) {
            new QListWidgetItem(urls[i], m_confUi.targetList);
        }
    }

}


/******************************************************************/
void KateCTagsConfigPage::delGlobalTagTarget()
{
    delete m_confUi.targetList->currentItem ();
}


/******************************************************************/
bool KateCTagsConfigPage::listContains(const QString &target)
{
    for (int i=0; i<m_confUi.targetList->count(); i++) {
        if (m_confUi.targetList->item(i)->text() == target) {
            return true;
        }
    }
    return false;
}

/******************************************************************/
void KateCTagsConfigPage::updateGlobalDB()
{
    if (m_proc.state() != QProcess::NotRunning) {
        return;
    }

    QString targets;
    for (int i=0; i<m_confUi.targetList->count(); i++) {
        targets += m_confUi.targetList->item(i)->text() + " ";
    }

    QString file = KStandardDirs::locateLocal("appdata", "plugins/katectags/common_db", true);

    if (targets.isEmpty()) {
        QFile::remove(file);
        return;
    }

    QString command = QString("%1 -f %2 %3").arg(m_confUi.cmdEdit->text()).arg(file).arg(targets) ;

    m_proc.setShellCommand(command);
    m_proc.setOutputChannelMode(KProcess::SeparateChannels);
    m_proc.start();

    if(!m_proc.waitForStarted(500)) {
        KMessageBox::error(0, i18n("Failed to run \"%1\". exitStatus = %2", command, m_proc.exitStatus()));
        return;
    }
    m_confUi.updateDB->setDisabled(true);
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

/******************************************************************/
void KateCTagsConfigPage::updateDone(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        KMessageBox::error(this, i18n("The CTags executable crashed."));
    }
    else if (exitCode != 0) {
        KMessageBox::error(this, i18n("The CTags command exited with code %1", exitCode));
    }
    
    m_confUi.updateDB->setDisabled(false);
    QApplication::restoreOverrideCursor();
}


