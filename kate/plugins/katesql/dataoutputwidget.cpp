/*
   Copyright (C) 2010  Marco Mentasti  <marcomentasti@gmail.com>

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

#include "dataoutputwidget.h"
#include "dataoutputmodel.h"
#include "dataoutputview.h"
#include "exportwizard.h"

#include <kate/application.h>
#include <kate/mainwindow.h>
#include <kate/documentmanager.h>
#include <ktexteditor/document.h>
#include <ktexteditor/view.h>
#include <kfiledialog.h>
#include <kapplication.h>
#include <ktoolbar.h>
#include <ktoggleaction.h>
#include <kaction.h>
#include <kicon.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <qheaderview.h>
#include <qlayout.h>
#include <qsqlquery.h>
#include <qsqlerror.h>
#include <qsize.h>
#include <qclipboard.h>
#include <qtextstream.h>
#include <qfile.h>

DataOutputWidget::DataOutputWidget(QWidget *parent)
: QWidget(parent)
, m_model(new DataOutputModel(this))
, m_view(new DataOutputView(this))
, m_isEmpty(true)
{
  m_view->setModel(m_model);

  QHBoxLayout *layout = new QHBoxLayout(this);
  m_dataLayout = new QVBoxLayout();

  KToolBar *toolbar = new KToolBar(this);
  toolbar->setOrientation(Qt::Vertical);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  toolbar->setIconSize(QSize(16, 16));

  /// TODO: disable actions if no results are displayed or selected

  KAction *action;

  action = new KAction( KIcon("distribute-horizontal-x"), i18nc("@action:intoolbar", "Resize columns to contents"), this);
  toolbar->addAction(action);
  connect(action, SIGNAL(triggered()), this, SLOT(resizeColumnsToContents()));

  action = new KAction( KIcon("distribute-vertical-y"), i18nc("@action:intoolbar", "Resize rows to contents"), this);
  toolbar->addAction(action);
  connect(action, SIGNAL(triggered()), this, SLOT(resizeRowsToContents()));

  action = new KAction( KIcon("edit-copy"), i18nc("@action:intoolbar", "Copy"), this);
  toolbar->addAction(action);
  m_view->addAction(action);
  connect(action, SIGNAL(triggered()), this, SLOT(slotCopySelected()));

  action = new KAction( KIcon("document-export-table"), i18nc("@action:intoolbar", "Export..."), this);
  toolbar->addAction(action);
  m_view->addAction(action);
  connect(action, SIGNAL(triggered()), this, SLOT(slotExport()));

  action = new KAction( KIcon("edit-clear"), i18nc("@action:intoolbar", "Clear"), this);
  toolbar->addAction(action);
  connect(action, SIGNAL(triggered()), this, SLOT(clearResults()));

  toolbar->addSeparator();

  KToggleAction *toggleAction = new KToggleAction( KIcon("applications-education-language"), i18nc("@action:intoolbar", "Use system locale"), this);
  toolbar->addAction(toggleAction);
  connect(toggleAction, SIGNAL(triggered()), this, SLOT(slotToggleLocale()));

  m_dataLayout->addWidget(m_view);

  layout->addWidget(toolbar);
  layout->addLayout(m_dataLayout);

  setLayout(layout);
}


DataOutputWidget::~DataOutputWidget()
{
}


void DataOutputWidget::showQueryResultSets(QSqlQuery &query)
{
  /// TODO: loop resultsets if > 1
  /// NOTE from Qt Documentation:
  /// When one of the statements is a non-select statement a count of affected rows
  /// may be available instead of a result set.

  if (!query.isSelect() || query.lastError().isValid())
    return;

  m_model->setQuery(query);

  m_isEmpty = false;

  resizeColumnsToContents();

  raise();
}


void DataOutputWidget::clearResults()
{
  // avoid crash when calling QSqlQueryModel::clear() after removing connection from the QSqlDatabase list
  if (m_isEmpty)
    return;

  m_model->clear();

  m_isEmpty = true;

  /// HACK needed to refresh headers. please correct if there's a better way
  m_view->horizontalHeader()->hide();
  m_view->verticalHeader()->hide();

  m_view->horizontalHeader()->show();
  m_view->verticalHeader()->show();
}


void DataOutputWidget::resizeColumnsToContents()
{
  if (m_model->rowCount() == 0)
    return;

  m_view->resizeColumnsToContents();
}


void DataOutputWidget::resizeRowsToContents()
{
  if (m_model->rowCount() == 0)
    return;

  m_view->resizeRowsToContents();

  int h = m_view->rowHeight(0);

  if (h > 0)
    m_view->verticalHeader()->setDefaultSectionSize(h);
}


void DataOutputWidget::slotToggleLocale()
{
  m_model->setUseSystemLocale(!m_model->useSystemLocale());
}


void DataOutputWidget::slotCopySelected()
{
  if (m_model->rowCount() <= 0)
    return;

  while (m_model->canFetchMore())
    m_model->fetchMore();

  if (!m_view->selectionModel()->hasSelection())
    m_view->selectAll();

  QString text;
  QTextStream stream(&text);

  exportData(stream);

  if (!text.isEmpty())
    kapp->clipboard()->setText(text);
}


void DataOutputWidget::slotExport()
{
  if (m_model->rowCount() <= 0)
    return;

  while (m_model->canFetchMore())
    m_model->fetchMore();

  if (!m_view->selectionModel()->hasSelection())
    m_view->selectAll();

  ExportWizard wizard(this);

  if (wizard.exec() != QDialog::Accepted)
    return;

  bool outputInDocument = wizard.field("outDocument").toBool();
  bool outputInClipboard = wizard.field("outClipboard").toBool();
  bool outputInFile = wizard.field("outFile").toBool();

  bool exportColumnNames = wizard.field("exportColumnNames").toBool();
  bool exportLineNumbers = wizard.field("exportLineNumbers").toBool();

  Options opt = NoOptions;

  if (exportColumnNames)
    opt |= ExportColumnNames;
  if (exportLineNumbers)
    opt |= ExportLineNumbers;

  bool quoteStrings = wizard.field("checkQuoteStrings").toBool();
  bool quoteNumbers = wizard.field("checkQuoteNumbers").toBool();

  QChar stringsQuoteChar = (quoteStrings) ? wizard.field("quoteStringsChar").toString().at(0) : '\0';
  QChar numbersQuoteChar = (quoteNumbers) ? wizard.field("quoteNumbersChar").toString().at(0) : '\0';

  QString fieldDelimiter = wizard.field("fieldDelimiter").toString();

  if (outputInDocument)
  {
    /* New document
    Kate::MainWindow *mw = Kate::application()->activeMainWindow();
    Kate::DocumentManager *dm = Kate::application()->documentManager();

    QString text;
    QTextStream stream(&text);

    exportData(stream, stringsQuoteChar, numbersQuoteChar, fieldDelimiter, opt);

    KTextEditor::Document *doc = dm->openUrl(KUrl());

    doc->setText(text);

    KTextEditor::View *kv = mw->activateView(doc);

    kv->setFocus();
    */

    Kate::MainWindow *mw = Kate::application()->activeMainWindow();
    KTextEditor::View *kv = mw->activeView();

    if (!kv)
      return;

    QString text;
    QTextStream stream(&text);

    exportData(stream, stringsQuoteChar, numbersQuoteChar, fieldDelimiter, opt);

    kv->insertText(text);
    kv->setFocus();
  }
  else if (outputInClipboard)
  {
    QString text;
    QTextStream stream(&text);

    exportData(stream, stringsQuoteChar, numbersQuoteChar, fieldDelimiter, opt);

    kapp->clipboard()->setText(text);
  }
  else if (outputInFile)
  {
    QString url = wizard.field("outFileUrl").toString();
    QFile data(url);
    if (data.open(QFile::WriteOnly | QFile::Truncate))
    {
      QTextStream stream(&data);

      exportData(stream, stringsQuoteChar, numbersQuoteChar, fieldDelimiter, opt);

      stream.flush();
    }
    else
    {
      KMessageBox::error(this, i18nc("@info", "Unable to open file <filename>%1</filename>").arg(url));
    }
  }
}


void DataOutputWidget::exportData(QTextStream &stream,
                                  const QChar stringsQuoteChar,
                                  const QChar numbersQuoteChar,
                                  const QString fieldDelimiter,
                                  const Options opt)
{
  QItemSelectionModel *selectionModel = m_view->selectionModel();

  if (!selectionModel->hasSelection())
    return;

  QString fixedFieldDelimiter = fieldDelimiter;

  /// FIXME: ugly workaround...
  fixedFieldDelimiter = fixedFieldDelimiter.replace("\\t", "\t");
  fixedFieldDelimiter = fixedFieldDelimiter.replace("\\r", "\r");
  fixedFieldDelimiter = fixedFieldDelimiter.replace("\\n", "\n");

  QTime t;
  t.start();

  QSet<int> columns;
  QSet<int> rows;
  QHash<QPair<int,int>,QString> snapshot;

  const QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

  snapshot.reserve(selectedIndexes.count());

  foreach (const QModelIndex& index, selectedIndexes)
  {
    const QVariant data = index.data(Qt::UserRole);

    const int col = index.column();
    const int row = index.row();

    if (!columns.contains(col))
      columns.insert(col);
    if (!rows.contains(row))
      rows.insert(row);

    if (data.type() < 7) // is numeric or boolean
    {
      if (numbersQuoteChar != '\0')
        snapshot[qMakePair(row,col)] = numbersQuoteChar + data.toString() + numbersQuoteChar;
      else
        snapshot[qMakePair(row,col)] = data.toString();
    }
    else
    {
      if (stringsQuoteChar != '\0')
        snapshot[qMakePair(row,col)] = stringsQuoteChar + data.toString() + stringsQuoteChar;
      else
        snapshot[qMakePair(row,col)] = data.toString();
    }
  }

  if (opt.testFlag(ExportColumnNames))
  {
    if (opt.testFlag(ExportLineNumbers))
      stream << fixedFieldDelimiter;

    QSetIterator<int> j(columns);
    while (j.hasNext())
    {
      const QVariant data = m_model->headerData(j.next(), Qt::Horizontal);

      if (stringsQuoteChar != '\0')
        stream << stringsQuoteChar + data.toString() + stringsQuoteChar;
      else
        stream << data.toString();

      if (j.hasNext())
        stream << fixedFieldDelimiter;
    }
    stream << "\n";
  }

  foreach(const int row, rows)
  {
    if (opt.testFlag(ExportLineNumbers))
      stream << row + 1 << fixedFieldDelimiter;

    QSetIterator<int> j(columns);
    while (j.hasNext())
    {
      stream << snapshot.value(qMakePair(row,j.next()));

      if (j.hasNext())
        stream << fixedFieldDelimiter;
    }
    stream << "\n";
  }

  kDebug() << "Export in" << t.elapsed() << "msecs";
}
