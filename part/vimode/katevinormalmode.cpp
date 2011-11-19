/*  This file is part of the KDE libraries and the Kate part.
 *
 *  Copyright (C) 2008-2009 Erlend Hamberg <ehamberg@gmail.com>
 *  Copyright (C) 2008 Evgeniy Ivanov <powerfox@kde.ru>
 *  Copyright (C) 2009 Paul Gideon Dann <pdgiddie@gmail.com>
 *  Copyright (C) 2011 Svyatoslav Kuzmich <svatoslav1@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "katevinormalmode.h"
#include "katevivisualmode.h"
#include "kateviinsertmode.h"
#include "kateviinputmodemanager.h"
#include "kateviglobal.h"
#include "kateglobal.h"
#include "katebuffer.h"
#include "kateviewhelpers.h"

#include <QApplication>
#include <QList>

using KTextEditor::Cursor;
using KTextEditor::Range;

#define ADDCMD(STR,FUNC, FLGS) m_commands.push_back( \
    new KateViCommand( this, STR, &KateViNormalMode::FUNC, FLGS ) );

#define ADDMOTION(STR, FUNC, FLGS) m_motions.push_back( new \
    KateViMotion( this, STR, &KateViNormalMode::FUNC, FLGS ) );

KateViNormalMode::KateViNormalMode( KateViInputModeManager *viInputModeManager, KateView * view,
    KateViewInternal * viewInternal ) : KateViModeBase()
{
  m_view = view;
  m_viewInternal = viewInternal;
  m_viInputModeManager = viInputModeManager;
  m_stickyColumn = -1;

  // FIXME: make configurable:
  m_extraWordCharacters = "";
  m_matchingItems["/*"] = "*/";
  m_matchingItems["*/"] = "-/*";

  m_matchItemRegex = generateMatchingItemRegex();

  m_defaultRegister = '"';

  m_scroll_count_limit = 1000; // Limit of count for scroll commands.
  m_timeoutlen = 1000; // FIXME: make configurable
  m_mappingKeyPress = false; // temporarily set to true when an aborted mapping sends key presses
  m_mappingTimer = new QTimer( this );
  connect(m_mappingTimer, SIGNAL(timeout()), this, SLOT(mappingTimerTimeOut()));

  initializeCommands();
  m_ignoreMapping = false;
  resetParser(); // initialise with start configuration
}

KateViNormalMode::~KateViNormalMode()
{
  qDeleteAll( m_commands );
  qDeleteAll( m_motions) ;
}

void KateViNormalMode::mappingTimerTimeOut()
{
  kDebug( 13070 ) << "timeout! key presses: " << m_mappingKeys;
  m_mappingKeyPress = true;
  m_viInputModeManager->feedKeyPresses( m_mappingKeys );
  m_mappingKeyPress = false;
  m_mappingKeys.clear();
}

/**
 * parses a key stroke to check if it's a valid (part of) a command
 * @return true if a command was completed and executed, false otherwise
 */
bool KateViNormalMode::handleKeypress( const QKeyEvent *e )
{
  int keyCode = e->key();
  QString text = e->text();

  // ignore modifier keys alone
  if ( keyCode == Qt::Key_Shift || keyCode == Qt::Key_Control
      || keyCode == Qt::Key_Alt || keyCode == Qt::Key_Meta ) {
    return false;
  }

  if ( keyCode == Qt::Key_Escape ) {
    reset();
    return true;
  }

  QChar key = KateViKeyParser::getInstance()->KeyEventToQChar( keyCode, text, e->modifiers(), e->nativeScanCode() );

  // check for matching mappings
  if ( !m_mappingKeyPress && !m_ignoreMapping) {
    m_mappingKeys.append( key );

    foreach ( const QString &str, getMappings() ) {
      if ( str.startsWith( m_mappingKeys ) ) {
        if ( str == m_mappingKeys ) {
          m_viInputModeManager->feedKeyPresses( getMapping( str ) );
          m_mappingTimer->stop();
          return true;
        } else {
          m_mappingTimer->start( m_timeoutlen );
          m_mappingTimer->setSingleShot( true );
          return true;
        }
      }
    }
    m_mappingKeys.clear();
  } else {
    // FIXME:
    //m_mappingKeyPress = false; // key press ignored wrt mappings, re-set m_mappingKeyPress
  }

  if ( m_ignoreMapping ) m_ignoreMapping = false;

  if ( key == 'f' || key == 'F' || key == 't' || key == 'T' || key == 'r' ) {
      // don't translate next character, we need the actual character so that
      // 'ab' is translated to 'fb' if the mapping 'a' -> 'f' exists
      m_ignoreMapping = true;
  }

  m_keysVerbatim.append( KateViKeyParser::getInstance()->decodeKeySequence( key ) );

  QChar c = QChar::Null;
  if ( m_keys.size() > 0 ) {
    c = m_keys.at( m_keys.size()-1 ); // last char
  }

  if ( ( keyCode >= Qt::Key_0 && keyCode <= Qt::Key_9 && c != '"' )       // key 0-9
      && ( m_countTemp != 0 || keyCode != Qt::Key_0 )                     // first digit can't be 0
      && ( c != 'f' && c != 't' && c != 'F' && c != 'T' && c != 'r' ) ) { // "find char" motions

    m_countTemp *= 10;
    m_countTemp += keyCode-Qt::Key_0;

    return true;
  } else if ( m_countTemp != 0 ) {
    m_count = getCount() * m_countTemp;
    m_countTemp = 0;
    m_iscounted = true;

    kDebug( 13070 ) << "count = " << getCount();
  }

  m_keys.append( key );

  // Special case: "cw" and "cW" work the same as "ce" and "cE" if the cursor is
  // on a non-blank.  This is because Vim interprets "cw" as change-word, and a
  // word does not include the following white space. (:help cw in vim)
  if ( ( m_keys == "cw" || m_keys == "cW" ) && !getCharUnderCursor().isSpace() ) {
    // Special case of the special case: :-)
    // If the cursor is at the end of the current word rewrite to "cl"
    Cursor c1( m_view->cursorPosition() ); // current position
    Cursor c2 = findWordEnd(c1.line(), c1.column()-1, true); // word end

    if ( c1 == c2 ) { // the cursor is at the end of a word
      m_keys = "cl";
    } else {
      if ( m_keys.at(1) == 'w' ) {
        m_keys = "ce";
      } else {
        m_keys = "cE";
      }
    }
  }

  if ( m_keys[ 0 ] == Qt::Key_QuoteDbl ) {
    if ( m_keys.size() < 2 ) {
      return true; // waiting for a register
    }
    else {
      QChar r = m_keys[ 1 ].toLower();

      if ( ( r >= '0' && r <= '9' ) || ( r >= 'a' && r <= 'z' ) || r == '_' || r == '+' || r == '*' ) {
        m_register = r;
        kDebug( 13070 ) << "Register set to " << r;
        m_keys.clear();
        return true;
      }
      else {
        resetParser();
        return true;
      }
    }
  }

  // if we have any matching commands so far, check which ones still match
  if ( m_matchingCommands.size() > 0 ) {
    int n = m_matchingCommands.size()-1;

    // remove commands not matching anymore
    for ( int i = n; i >= 0; i-- ) {
      if ( !m_commands.at( m_matchingCommands.at( i ) )->matches( m_keys ) ) {
        //kDebug( 13070 ) << "removing " << m_commands.at( m_matchingCommands.at( i ) )->pattern() << ", size before remove is " << m_matchingCommands.size();
        if ( m_commands.at( m_matchingCommands.at( i ) )->needsMotion() ) {
          // "cache" command needing a motion for later
          //kDebug( 13070 ) << "m_motionOperatorIndex set to " << m_motionOperatorIndex;
          m_motionOperatorIndex = m_matchingCommands.at( i );
        }
        m_matchingCommands.remove( i );
      }
    }

    // check if any of the matching commands need a motion/text object, if so
    // push the current command length to m_awaitingMotionOrTextObject so one
    // knows where to split the command between the operator and the motion
    for ( int i = 0; i < m_matchingCommands.size(); i++ ) {
      if ( m_commands.at( m_matchingCommands.at( i ) )->needsMotion() ) {
        m_awaitingMotionOrTextObject.push( m_keys.size() );
        break;
      }
    }
  } else {
    // go through all registered commands and put possible matches in m_matchingCommands
    for ( int i = 0; i < m_commands.size(); i++ ) {
      if ( m_commands.at( i )->matches( m_keys ) ) {
        m_matchingCommands.push_back( i );
        if ( m_commands.at( i )->needsMotion() && m_commands.at( i )->pattern().length() == m_keys.size() ) {
          m_awaitingMotionOrTextObject.push( m_keys.size() );
        }
      }
    }
  }

  // this indicates where in the command string one should start looking for a motion command
  int checkFrom = ( m_awaitingMotionOrTextObject.isEmpty() ? 0 : m_awaitingMotionOrTextObject.top() );

  //kDebug( 13070 ) << "checkFrom: " << checkFrom;

  // look for matching motion commands from position 'checkFrom'
  // FIXME: if checkFrom hasn't changed, only motions whose index is in
  // m_matchingMotions shold be checked
  if ( checkFrom < m_keys.size() ) {
    for ( int i = 0; i < m_motions.size(); i++ ) {
      //kDebug( 13070 )  << "\tchecking " << m_keys.mid( checkFrom )  << " against " << m_motions.at( i )->pattern();
      if ( m_motions.at( i )->matches( m_keys.mid( checkFrom ) ) ) {
        //kDebug( 13070 )  << m_keys.mid( checkFrom ) << " matches!";
        m_matchingMotions.push_back( i );

        // if it matches exact, we have found the motion command to execute
        if ( m_motions.at( i )->matchesExact( m_keys.mid( checkFrom ) ) ) {
          if ( checkFrom == 0 ) {
            // no command given before motion, just move the cursor to wherever
            // the motion says it should go to
            KateViRange r = m_motions.at( i )->execute();

            // jump over folding regions since we are just moving the cursor
            int currLine = m_view->cursorPosition().line();
            int delta = r.endLine - currLine;
            int vline = doc()->foldingTree()->getVirtualLine( currLine );
            r.endLine = doc()->foldingTree()->getRealLine( vline+delta );
            if ( r.endLine >= doc()->lines() ) r.endLine = doc()->lines()-1;

            // make sure the position is valid before moving the cursor there
            // TODO: can this be simplified? :/
            if ( r.valid
                && r.endLine >= 0
                && ( r.endLine == 0 || r.endLine <= doc()->lines()-1 )
                && r.endColumn >= 0 ) {
               if ( r.endColumn >= doc()->lineLength( r.endLine )
                   && doc()->lineLength( r.endLine ) > 0 ) {
                   r.endColumn = doc()->lineLength( r.endLine ) - 1;
               }

              kDebug( 13070 ) << "No command given, going to position ("
                << r.endLine << "," << r.endColumn << ")";
              goToPos( r );
              m_viInputModeManager->clearLog();
            } else {
              kDebug( 13070 ) << "Invalid position: (" << r.endLine << "," << r.endColumn << ")";
            }

            resetParser();

            // if normal mode was started by using Ctrl-O in insert mode,
            // it's time to go back to insert mode.
            if (m_viInputModeManager->getTemporaryNormalMode()) {
                m_viInputModeManager->setTemporaryNormalMode(false);
                startInsertMode();
                m_viewInternal->repaint();
            }

            return true;
          } else {
            // execute the specified command and supply the position returned from
            // the motion

            m_commandRange = m_motions.at( i )->execute();
            m_linewiseCommand = m_motions.at( i )->isLineWise();

            // if we didn't get an explicit start position, use the current cursor position
            if ( m_commandRange.startLine == -1 ) {
              Cursor c( m_view->cursorPosition() );
              m_commandRange.startLine = c.line();
              m_commandRange.startColumn = c.column();
            }

            // special case: When using the "w" motion in combination with an operator and
            // the last word moved over is at the end of a line, the end of that word
            // becomes the end of the operated text, not the first word in the next line.
            if ( m_keys.right(1) == "w" || m_keys.right(1) == "W" ) {
               if(m_commandRange.endLine != m_commandRange.startLine &&
                   m_commandRange.endColumn == getLine(m_commandRange.endLine).indexOf( QRegExp("\\S") )){
                     m_commandRange.endLine--;
                     m_commandRange.endColumn = doc()->lineLength(m_commandRange.endLine );
                   }
            }

            m_commandWithMotion = true;

            if ( m_commandRange.valid ) {
              kDebug( 13070 ) << "Run command" << m_commands.at( m_motionOperatorIndex )->pattern()
                << "from (" << m_commandRange.startLine << "," << m_commandRange.endLine << ")"
                << "to (" << m_commandRange.endLine << "," << m_commandRange.endColumn << ")";
              executeCommand( m_commands.at( m_motionOperatorIndex ) );
            } else {
              kDebug( 13070 ) << "Invalid range: "
                << "from (" << m_commandRange.startLine << "," << m_commandRange.endLine << ")"
                << "to (" << m_commandRange.endLine << "," << m_commandRange.endColumn << ")";
            }

            m_commandWithMotion = false;
            reset();
            return true;
          }
        }
      }
    }
  }

  //kDebug( 13070 ) << "'" << m_keys << "' MATCHING COMMANDS: " << m_matchingCommands.size();
  //kDebug( 13070 ) << "'" << m_keys << "' MATCHING MOTIONS: " << m_matchingMotions.size();
  //kDebug( 13070 ) << "'" << m_keys << "' AWAITING MOTION OR TO (INDEX): " << ( m_awaitingMotionOrTextObject.isEmpty() ? 0 : m_awaitingMotionOrTextObject.top() );

  // if we have only one match, check if it is a perfect match and if so, execute it
  // if it's not waiting for a motion or a text object
  if ( m_matchingCommands.size() == 1 ) {
    if ( m_commands.at( m_matchingCommands.at( 0 ) )->matchesExact( m_keys )
        && !m_commands.at( m_matchingCommands.at( 0 ) )->needsMotion() ) {
      //kDebug( 13070 ) << "Running command at index " << m_matchingCommands.at( 0 );

      KateViCommand *cmd = m_commands.at( m_matchingCommands.at( 0 ) );
      executeCommand( cmd );

      // check if reset() should be called. some commands in visual mode should not end visual mode
      if ( cmd->shouldReset() ) {
        reset();
      }
      resetParser();

      return true;
    }
  } else if ( m_matchingCommands.size() == 0 && m_matchingMotions.size() == 0 ) {
      resetParser();
      return false;
  }

  m_matchingMotions.clear();
  return false;
}

/**
 * (re)set to start configuration. This is done when a command is completed
 * executed or when a command is aborted
 */
void KateViNormalMode::resetParser()
{
//  kDebug( 13070 ) << "***RESET***";
  m_keys.clear();
  m_keysVerbatim.clear();
  m_count = 0;
  m_iscounted = false;
  m_countTemp = 0;
  m_register = QChar::Null;
  m_findWaitingForChar = false;
  m_matchingCommands.clear();
  m_matchingMotions.clear();
  m_awaitingMotionOrTextObject.clear();
  m_motionOperatorIndex = 0;

  m_commandWithMotion = false;
  m_linewiseCommand = true;
  m_deleteCommand = false;

}

// reset the command parser
void KateViNormalMode::reset()
{
    resetParser();
    m_commandRange.startLine = -1;
    m_commandRange.startColumn = -1;
}

void KateViNormalMode::goToPos( const KateViRange &r )
{
  Cursor c;
  c.setLine( r.endLine );
  c.setColumn( r.endColumn );

  if ( r.jump ) {
    addCurrentPositionToJumpList();
  }

  if ( c.line() >= doc()->lines() ) {
    c.setLine( doc()->lines()-1 );
  }

  updateCursor( c );
}

void KateViNormalMode::executeCommand( const KateViCommand* cmd )
{
  cmd->execute();

  // if normal mode was started by using Ctrl-O in insert mode,
  // it's time to go back to insert mode.
  if (m_viInputModeManager->getTemporaryNormalMode()) {
      m_viInputModeManager->setTemporaryNormalMode(false);
      startInsertMode();
      m_viewInternal->repaint();
  }

  // if the command was a change, and it didn't enter insert mode, store the key presses so that
  // they can be repeated with '.'
  if ( m_viInputModeManager->getCurrentViMode() != InsertMode ) {
    if ( cmd->isChange() && !m_viInputModeManager->isRunningMacro() ) {
      m_viInputModeManager->storeChangeCommand();
    }

    m_viInputModeManager->clearLog();
  }

  // make sure the cursor does not end up after the end of the line
  Cursor c( m_view->cursorPosition() );
  if ( m_viInputModeManager->getCurrentViMode() == NormalMode ) {
    int lineLength = doc()->lineLength( c.line() );

    if ( c.column() >= lineLength ) {
      if ( lineLength == 0 ) {
        c.setColumn( 0 );
      } else {
        c.setColumn( lineLength-1 );
      }
    }
    updateCursor( c );
  }
}

void KateViNormalMode::addCurrentPositionToJumpList() {
    m_viInputModeManager->addJump(m_view->cursorPosition());
}

////////////////////////////////////////////////////////////////////////////////
// COMMANDS AND OPERATORS
////////////////////////////////////////////////////////////////////////////////

/**
 * enter insert mode at the cursor position
 */

bool KateViNormalMode::commandEnterInsertMode()
{
  m_stickyColumn = -1;
  return startInsertMode();
}

/**
 * enter insert mode after the current character
 */

bool KateViNormalMode::commandEnterInsertModeAppend()
{
  Cursor c( m_view->cursorPosition() );
  c.setColumn( c.column()+1 );

  // if empty line, the cursor should start at column 0
  if ( doc()->lineLength( c.line() ) == 0 ) {
    c.setColumn( 0 );
  }

  // cursor should never be in a column > number of columns
  if ( c.column() > doc()->lineLength( c.line() ) ) {
    c.setColumn( doc()->lineLength( c.line() ) );
  }

  updateCursor( c );

  m_stickyColumn = -1;
  return startInsertMode();
}

/**
 * start insert mode after the last character of the line
 */

bool KateViNormalMode::commandEnterInsertModeAppendEOL()
{
  Cursor c( m_view->cursorPosition() );
  c.setColumn( doc()->lineLength( c.line() ) );
  updateCursor( c );

  m_stickyColumn = -1;
  return startInsertMode();
}

bool KateViNormalMode::commandEnterInsertModeBeforeFirstNonBlankInLine()
{
  Cursor cursor( m_view->cursorPosition() );
  QRegExp nonSpace( "\\S" );
  int c = getLine().indexOf( nonSpace );
  if ( c == -1 ) {
    c = 0;
  }
  cursor.setColumn( c );
  updateCursor( cursor );

  m_stickyColumn = -1;
  return startInsertMode();
}

bool KateViNormalMode::commandEnterVisualLineMode()
{
  if ( m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
    reset();
    return true;
  }

  return startVisualLineMode();
}

bool KateViNormalMode::commandEnterVisualBlockMode()
{
  if ( m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    reset();
    return true;
  }

  return startVisualBlockMode();
}

bool KateViNormalMode::commandReselectVisual()
{
  // start last visual mode and set start = `< and cursor = `>
  Cursor c1 = m_view->getViInputModeManager()->getMarkPosition( '<' );
  Cursor c2 = m_view->getViInputModeManager()->getMarkPosition( '>' );

  // we should either get two valid cursors or two invalid cursors
  Q_ASSERT( c1.isValid() == c2.isValid() );

  if ( c1.isValid() && c2.isValid() ) {
    m_viInputModeManager->getViVisualMode()->setStart( c1 );
    updateCursor( c2 );

    switch ( m_viInputModeManager->getViVisualMode()->getLastVisualMode() ) {
    case VisualMode:
      return commandEnterVisualMode();
      break;
    case VisualLineMode:
      return commandEnterVisualLineMode();
      break;
    case VisualBlockMode:
      return commandEnterVisualBlockMode();
      break;
    default:
      Q_ASSERT( "invalid visual mode" );
    }
  } else {
    error("No previous visual selection");
  }

  return false;
}

bool KateViNormalMode::commandEnterVisualMode()
{
  if ( m_viInputModeManager->getCurrentViMode() == VisualMode ) {
    reset();
    return true;
  }

  return startVisualMode();
}

bool KateViNormalMode::commandToOtherEnd()
{
  if ( m_viInputModeManager->getCurrentViMode() == VisualMode
      || m_viInputModeManager->getCurrentViMode() == VisualLineMode
      || m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    m_viInputModeManager->getViVisualMode()->switchStartEnd();
    return true;
  }

  return false;
}

bool KateViNormalMode::commandEnterReplaceMode()
{
  return startReplaceMode();
}

bool KateViNormalMode::commandDeleteLine()
{
  Cursor c( m_view->cursorPosition() );

  KateViRange r;

  r.startLine = c.line();
  r.endLine = c.line()+getCount()-1;

  int column = c.column();

  bool ret = deleteRange( r, LineWise );

  c = m_view->cursorPosition();
  if ( column > doc()->lineLength( c.line() )-1 ) {
    column = doc()->lineLength( c.line() )-1;
  }
  if ( column < 0 ) {
    column = 0;
  }

  if ( c.line() > doc()->lines()-1 ) {
    c.setLine( doc()->lines()-1 );
  }

  c.setColumn( column );
  m_stickyColumn = -1;
  updateCursor( c );

  m_deleteCommand = true;
  return ret;
}

bool KateViNormalMode::commandDelete()
{
  m_deleteCommand = true;
  return deleteRange( m_commandRange, getOperationMode() );
}

bool KateViNormalMode::commandDeleteToEOL()
{
  Cursor c( m_view->cursorPosition() );
  OperationMode m = CharWise;

  if ( m_viInputModeManager->getCurrentViMode() == NormalMode ) {
    m_commandRange.startLine = c.line();
    m_commandRange.startColumn = c.column();
    m_commandRange.endLine = c.line()+getCount()-1;
    m_commandRange.endColumn = doc()->lineLength( m_commandRange.endLine )-1;
  }

  if ( m_viInputModeManager->getCurrentViMode() == VisualMode
      || m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
    m = LineWise;
  } else if ( m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    m_commandRange.normalize();
    m_commandRange.endColumn = KateVi::EOL;
    m = Block;
  }

  bool r = deleteRange( m_commandRange, m );

  switch (m) {
  case CharWise:
    c.setColumn( doc()->lineLength( c.line() )-1 );
    break;
  case LineWise:
    c.setLine( m_commandRange.startLine );
    c.setColumn( 0 ); // FIXME: should be first non-blank
    break;
  case Block:
    c.setLine( m_commandRange.startLine );
    c.setColumn( m_commandRange.startColumn-1 );
    break;
  }

  // make sure cursor position is valid after deletion
  if ( c.line() < 0 ) {
    c.setLine( 0 );
  }
  if ( c.column() > doc()->lineLength( c.line() )-1 ) {
    c.setColumn( doc()->lineLength( c.line() )-1 );
  }
  if ( c.column() < 0 ) {
    c.setColumn( 0 );
  }

  updateCursor( c );

  m_deleteCommand = true;
  return r;
}

bool KateViNormalMode::commandMakeLowercase()
{
  Cursor c = m_view->cursorPosition();

  OperationMode m = getOperationMode();
  QString text = getRange( m_commandRange, m );
  if (m == LineWise)
    text = text.left(text.size() - 1); // don't need '\n' at the end;
  QString lowerCase = text.toLower();

  m_commandRange.normalize();
  Cursor start( m_commandRange.startLine, m_commandRange.startColumn );
  Cursor end( m_commandRange.endLine, m_commandRange.endColumn );
  Range range( start, end );

  doc()->replaceText( range, lowerCase, m == Block );

  if (m_viInputModeManager->getCurrentViMode() == NormalMode)
    updateCursor( start );
  else
    updateCursor(c);

  return true;
}

bool KateViNormalMode::commandMakeLowercaseLine()
{
  Cursor c( m_view->cursorPosition() );

  m_commandRange.startLine = c.line();
  m_commandRange.endLine = c.line() + getCount() - 1;
  m_commandRange.startColumn = 0;
  m_commandRange.endColumn = doc()->lineLength( c.line() )-1;

  return commandMakeLowercase();
}

bool KateViNormalMode::commandMakeUppercase()
{
  Cursor c = m_view->cursorPosition();
  OperationMode m = getOperationMode();
  QString text = getRange( m_commandRange, m );
  if (m == LineWise)
    text = text.left(text.size() - 1); // don't need '\n' at the end;
  QString upperCase = text.toUpper();

  m_commandRange.normalize();
  Cursor start( m_commandRange.startLine, m_commandRange.startColumn );
  Cursor end( m_commandRange.endLine, m_commandRange.endColumn );
  Range range( start, end );

  doc()->replaceText( range, upperCase, m == Block );
  if (m_viInputModeManager->getCurrentViMode() == NormalMode)
    updateCursor( start );
  else
    updateCursor(c);

  return true;
}

bool KateViNormalMode::commandMakeUppercaseLine()
{
  Cursor c( m_view->cursorPosition() );

  m_commandRange.startLine = c.line();
  m_commandRange.endLine = c.line() + getCount() - 1;
  m_commandRange.startColumn = 0;
  m_commandRange.endColumn = doc()->lineLength( c.line() )-1;

  return commandMakeUppercase();
}

bool KateViNormalMode::commandChangeCase()
{
  switchView();
  QString text;
  Range range;
  Cursor c( m_view->cursorPosition() );

  // in visual mode, the range is from start position to end position...
  if ( m_viInputModeManager->getCurrentViMode() == VisualMode
    || m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    Cursor c2 = m_viInputModeManager->getViVisualMode()->getStart();

    if ( c2 > c ) {
      c2.setColumn( c2.column()+1 );
    } else {
      c.setColumn( c.column()+1 );
    }

    range.setRange( c, c2 );
  // ... in visual line mode, the range is from column 0 on the first line to
  // the line length of the last line...
  } else if ( m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
    Cursor c2 = m_viInputModeManager->getViVisualMode()->getStart();

    if ( c2 > c ) {
      c2.setColumn( doc()->lineLength( c2.line() ) );
      c.setColumn( 0 );
    } else {
      c.setColumn( doc()->lineLength( c.line() ) );
      c2.setColumn( 0 );
    }

    range.setRange( c, c2 );
  // ... and in normal mode the range is from the current position to the
  // current position + count
  } else {
    Cursor c2 = c;
    c2.setColumn( c.column()+getCount() );

    if ( c2.column() > doc()->lineLength( c.line() ) ) {
      c2.setColumn( doc()->lineLength( c.line() ) );
    }

    range.setRange( c, c2 );
  }

  bool block = m_viInputModeManager->getCurrentViMode() == VisualBlockMode;

  // get the text the command should operate on
  text = doc()->text ( range, block );

  // for every character, switch its case
  for ( int i = 0; i < text.length(); i++ ) {
    if ( text.at(i).isUpper() ) {
      text[i] = text.at(i).toLower();
    } else if ( text.at(i).isLower() ) {
      text[i] = text.at(i).toUpper();
    }
  }

  // replace the old text with the modified text
  doc()->replaceText( range, text, block );

  // in normal mode, move the cursor to the right, in visual mode move the
  // cursor to the start of the selection
  if ( m_viInputModeManager->getCurrentViMode() == NormalMode ) {
    updateCursor( range.end() );
  } else {
    updateCursor( range.start() );
  }

  return true;
}

bool KateViNormalMode::commandOpenNewLineUnder()
{
  Cursor c( m_view->cursorPosition() );

  c.setColumn( doc()->lineLength( c.line() ) );
  updateCursor( c );

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    doc()->newLine( m_view );
  }

  m_stickyColumn = -1;
  startInsertMode();
  m_viewInternal->repaint ();

  return true;
}

bool KateViNormalMode::commandOpenNewLineOver()
{
  Cursor c( m_view->cursorPosition() );

  if ( c.line() == 0 ) {
    for (unsigned int i = 0; i < getCount(); i++ ) {
      doc()->insertLine( 0, QString() );
    }
    c.setColumn( 0 );
    c.setLine( 0 );
    updateCursor( c );
  } else {
    c.setLine( c.line()-1 );
    c.setColumn( getLine( c.line() ).length() );
    updateCursor( c );
    for ( unsigned int i = 0; i < getCount(); i++ ) {
        doc()->newLine( m_view );
    }

    if ( getCount() > 1 ) {
      c = m_view->cursorPosition();
      c.setLine( c.line()-(getCount()-1 ) );
      updateCursor( c );
    }
    //c.setLine( c.line()-getCount() );
  }

  m_stickyColumn = -1;
  startInsertMode();
  m_viewInternal->repaint ();

  return true;
}

bool KateViNormalMode::commandJoinLines()
{
  Cursor c( m_view->cursorPosition() );

  // remember line length so the cursor can be put between the joined lines
  int l = doc()->lineLength( c.line() );

  unsigned int from = c.line();
  unsigned int to = c.line()+getCount();

  // if we were given a range of lines, this information overrides the previous
  if ( m_commandRange.startLine != -1 && m_commandRange.endLine != -1 ) {
    m_commandRange.normalize();
    c.setLine ( m_commandRange.startLine );
    to = m_commandRange.endLine;
  }

  joinLines( from, to );

  // position cursor between the joined lines
  c.setColumn( l );
  updateCursor( c );

  m_deleteCommand = true;
  return true;
}

bool KateViNormalMode::commandChange()
{
  Cursor c( m_view->cursorPosition() );

  OperationMode m = getOperationMode();

  doc()->editStart();
  commandDelete();

  // if we deleted several lines, insert an empty line and put the cursor there
  if ( m == LineWise ) {
    doc()->insertLine( m_commandRange.startLine, QString() );
    c.setLine( m_commandRange.startLine );
    c.setColumn(0);
  }
  doc()->editEnd();

  if ( m == LineWise ) {
    updateCursor( c );
  }

  // block substitute can be simulated by first deleting the text (done above) and then starting
  // block prepend
  if ( m == Block ) {
    return commandPrependToBlock();
  }

  commandEnterInsertMode();

  // correct indentation level
  if ( m == LineWise ) {
    m_view->align();
  }

  m_deleteCommand = true;
  return true;
}

bool KateViNormalMode::commandChangeToEOL()
{
  commandDeleteToEOL();

  if ( getOperationMode() == Block ) {
    return commandPrependToBlock();
  }

  m_deleteCommand = true;
  return commandEnterInsertModeAppend();
}

bool KateViNormalMode::commandChangeLine()
{
  m_deleteCommand = true;
  Cursor c( m_view->cursorPosition() );
  c.setColumn( 0 );
  updateCursor( c );

  doc()->editStart();

  // if count >= 2 start by deleting the whole lines
  if ( getCount() >= 2 ) {
    KateViRange r( c.line(), 0, c.line()+getCount()-2, 0, ViMotion::InclusiveMotion );
    deleteRange( r );
  }

  // ... then delete the _contents_ of the last line, but keep the line
  KateViRange r( c.line(), c.column(), c.line(), doc()->lineLength( c.line() )-1,
      ViMotion::InclusiveMotion );
  deleteRange( r, CharWise, true );
  doc()->editEnd();

  // ... then enter insert mode
  if ( getOperationMode() == Block ) {
    return commandPrependToBlock();
  }
  commandEnterInsertModeAppend();

  // correct indentation level
  m_view->align();

  return true;
}

bool KateViNormalMode::commandSubstituteChar()
{
  if ( commandDeleteChar() ) {
    return commandEnterInsertMode();
  }

  m_deleteCommand = true;
  return false;
}

bool KateViNormalMode::commandSubstituteLine()
{
  m_deleteCommand = true;
  return commandChangeLine();
}

bool KateViNormalMode::commandYank()
{
  Cursor c( m_view->cursorPosition() );

  bool r = false;
  QString yankedText;

  OperationMode m = getOperationMode();
  yankedText = getRange( m_commandRange, m );

  fillRegister( getChosenRegister( '0' ), yankedText, m );

  return r;
}

bool KateViNormalMode::commandYankLine()
{
  Cursor c( m_view->cursorPosition() );
  QString lines;
  int linenum = c.line();

  for ( unsigned int i = 0; i < getCount(); i++ ) {
      lines.append( getLine( linenum + i ) + '\n' );
  }
  fillRegister( getChosenRegister( '0' ), lines, LineWise );

  return true;
}

bool KateViNormalMode::commandYankToEOL()
{
  Cursor c( m_view->cursorPosition() );

  bool r = false;
  QString yankedText;

  m_commandRange.endLine = c.line()+getCount()-1;
  m_commandRange.endColumn = doc()->lineLength( m_commandRange.endLine )-1;

  OperationMode m = CharWise;

  if ( m_viInputModeManager->getCurrentViMode() == VisualMode
      || m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
    m = LineWise;
    KateViVisualMode* visualmode = ((KateViVisualMode*) this);
    visualmode->setStart( Cursor(visualmode->getStart().line(),0) );
  } else if (m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    m = Block;;
  }

  if ( m_viInputModeManager->getCurrentViMode() == NormalMode ) {
    m_commandRange.startLine = c.line();
    m_commandRange.startColumn = c.column();
  }

  yankedText = getRange( m_commandRange, m );

  fillRegister( getChosenRegister( '0' ), yankedText, m );

  return r;
}

// insert the text in the given register at the cursor position
// the cursor should end up at the beginning of what was pasted
bool KateViNormalMode::commandPaste()
{
  Cursor c( m_view->cursorPosition() );
  Cursor cAfter = c;
  QChar reg = getChosenRegister( m_defaultRegister );

  OperationMode m = getRegisterFlag( reg );
  QString textToInsert = getRegisterContent( reg );

  if ( textToInsert.isNull() ) {
    error(i18n("Nothing in register %1", reg ));
    return false;
  }

  if ( getCount() > 1 ) {
    textToInsert = textToInsert.repeated( getCount() ); // FIXME: does this make sense for blocks?
  }

  if ( m == LineWise ) {
    textToInsert.chop( 1 ); // remove the last \n
    c.setColumn( doc()->lineLength( c.line() ) ); // paste after the current line and ...
    textToInsert.prepend( QChar( '\n' ) ); // ... prepend a \n, so the text starts on a new line

    cAfter.setLine( cAfter.line()+1 );
    cAfter.setColumn( 0 );
  } else {
    if ( getLine( c.line() ).length() > 0 ) {
      c.setColumn( c.column()+1 );
    }

    cAfter = c;
  }

  doc()->insertText( c, textToInsert, m == Block );

  updateCursor( cAfter );

  return true;
}

// insert the text in the given register before the cursor position
// the cursor should end up at the beginning of what was pasted
bool KateViNormalMode::commandPasteBefore()
{
  Cursor c( m_view->cursorPosition() );
  Cursor cAfter = c;
  QChar reg = getChosenRegister( m_defaultRegister );

  QString textToInsert = getRegisterContent( reg );
  OperationMode m = getRegisterFlag( reg );

  if ( getCount() > 1 ) {
    textToInsert = textToInsert.repeated( getCount() );
  }

  if ( textToInsert.endsWith('\n') ) { // lines
    c.setColumn( 0 );
    cAfter.setColumn( 0 );
  }

  doc()->insertText( c, textToInsert, m == Block );

  updateCursor( cAfter );

  return true;
}

bool KateViNormalMode::commandDeleteChar()
{
    Cursor c( m_view->cursorPosition() );
    KateViRange r( c.line(), c.column(), c.line(), c.column()+getCount(), ViMotion::ExclusiveMotion );

    if ( m_commandRange.startLine != -1 && m_commandRange.startColumn != -1 ) {
      r = m_commandRange;
    } else {
      if ( r.endColumn > doc()->lineLength( r.startLine ) ) {
        r.endColumn = doc()->lineLength( r.startLine );
      }
    }

    // should delete entire lines if in visual line mode and selection in visual block mode
    OperationMode m = CharWise;
    if ( m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
      m = LineWise;
    } else if ( m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
      m = Block;
    }

    m_deleteCommand = true;
    return deleteRange( r, m );
}

bool KateViNormalMode::commandDeleteCharBackward()
{
    Cursor c( m_view->cursorPosition() );

    KateViRange r( c.line(), c.column()-getCount(), c.line(), c.column(), ViMotion::ExclusiveMotion );

    if ( m_commandRange.startLine != -1 && m_commandRange.startColumn != -1 ) {
      r = m_commandRange;
    } else {
      if ( r.startColumn < 0 ) {
        r.startColumn = 0;
      }
    }

    // should delete entire lines if in visual line mode and selection in visual block mode
    OperationMode m = CharWise;
    if ( m_viInputModeManager->getCurrentViMode() == VisualLineMode ) {
      m = LineWise;
    } else if ( m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
      m = Block;
    }

    m_deleteCommand = true;
    return deleteRange( r, m );
}

bool KateViNormalMode::commandReplaceCharacter()
{

bool r;
if ( m_viInputModeManager->getCurrentViMode() == VisualMode
      || m_viInputModeManager->getCurrentViMode() == VisualLineMode
      || m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {

    OperationMode m = getOperationMode();
    QString text = getRange( m_commandRange, m );

    if (m == LineWise)
      text = text.left(text.size() - 1); // don't need '\n' at the end;

    text.replace( QRegExp( "[^\n]" ), m_keys.right( 1 ) );

    m_commandRange.normalize();
    Cursor start( m_commandRange.startLine, m_commandRange.startColumn );
    Cursor end( m_commandRange.endLine, m_commandRange.endColumn );
    Range range( start, end );

    r = doc()->replaceText( range, text, m == Block );

} else {
    Cursor c1( m_view->cursorPosition() );
    Cursor c2( m_view->cursorPosition() );

    c2.setColumn( c2.column()+1 );

    r = doc()->replaceText( Range( c1, c2 ), m_keys.right( 1 ) );
    updateCursor( c1 );

}
  m_ignoreMapping = false;

  return r;
}

bool KateViNormalMode::commandSwitchToCmdLine()
{
    Cursor c( m_view->cursorPosition() );

    m_view->switchToCmdLine();

    if ( m_viInputModeManager->getCurrentViMode() == VisualMode
      || m_viInputModeManager->getCurrentViMode() == VisualLineMode
      || m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
      // if in visual mode, make command range == visual selection
      m_viInputModeManager->getViVisualMode()->saveRangeMarks();
      m_view->cmdLineBar()->setText( "'<,'>" );
    }
    else if ( getCount() != 1 ) {
      // if a count is given, the range [current line] to [current line] +
      // count should be prepended to the command line
      m_view->cmdLineBar()->setText( ".,.+" +QString::number( getCount()-1 ), false);
    }

    return true;
}

bool KateViNormalMode::commandSearch()
{
    m_view->find();
    return true;
}

bool KateViNormalMode::commandUndo()
{
    doc()->undo();
    return true;
}

bool KateViNormalMode::commandRedo()
{
    doc()->redo();
    return true;
}

bool KateViNormalMode::commandSetMark()
{
  Cursor c( m_view->cursorPosition() );

  m_view->getViInputModeManager()->addMark( doc(), m_keys.at( m_keys.size()-1 ), c );
  kDebug( 13070 ) << "set mark at (" << c.line() << "," << c.column() << ")";

  return true;
}

bool KateViNormalMode::commandIndentLine()
{
    Cursor c( m_view->cursorPosition() );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        doc()->indent( KTextEditor::Range( c.line()+i, 0, c.line()+i, 0), 1 );
    }

    return true;
}

bool KateViNormalMode::commandUnindentLine()
{
    Cursor c( m_view->cursorPosition() );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        doc()->indent( KTextEditor::Range( c.line()+i, 0, c.line()+i, 0), -1 );
    }

    return true;
}

bool KateViNormalMode::commandIndentLines()
{
  Cursor c( m_view->cursorPosition() );

  m_commandRange.normalize();

  int line1 = m_commandRange.startLine;
  int line2 = m_commandRange.endLine;
  int col = getLine( line2 ).length();

  doc()->indent( KTextEditor::Range( line1, 0, line2, col ), getCount() );

  return true;
}

bool KateViNormalMode::commandUnindentLines()
{
  Cursor c( m_view->cursorPosition() );

  m_commandRange.normalize();

  int line1 = m_commandRange.startLine;
  int line2 = m_commandRange.endLine;

  doc()->indent( KTextEditor::Range( line1, 0, line2, doc()->lineLength( line2 ) ), -getCount() );

  return true;
}

bool KateViNormalMode::commandScrollPageDown()
{
  if ( getCount() < m_scroll_count_limit ) {

    for(uint i = 0; i < getCount(); i++)
      m_view->pageDown();
  }
  return true;
}

bool KateViNormalMode::commandScrollPageUp()
{
  if ( getCount() < m_scroll_count_limit ) {
    for(uint i=0; i < getCount(); i++)
      m_view->pageUp();
  }
  return true;

}

bool KateViNormalMode::commandScrollHalfPageUp()
{
  if ( getCount() < m_scroll_count_limit ) {

    for(uint i=0; i < getCount(); i++)
      m_viewInternal->pageUp(false, true);
  }
  return true;
}

bool KateViNormalMode::commandScrollHalfPageDown()
{
  if ( getCount() < m_scroll_count_limit ) {
    for(uint i=0; i < getCount(); i++)
      m_viewInternal->pageDown(false, true);
  }
  return true;
}

bool KateViNormalMode::commandCentreViewOnCursor()
{
  Cursor c( m_view->cursorPosition() );
  int linesToScroll = (m_viewInternal->endLine()-linesDisplayed()/2)-c.line();

  scrollViewLines( -linesToScroll );

  return true;
}

bool KateViNormalMode::commandAbort()
{
  reset();
  return true;
}

bool KateViNormalMode::commandPrintCharacterCode()
{
  QChar ch = getCharUnderCursor();

  if ( ch == QChar::Null ) {
      message( QString( "NUL" ) );
  } else {

    int code = ch.unicode();

    QString dec = QString::number( code );
    QString hex = QString::number( code, 16 );
    QString oct = QString::number( code, 8 );
    if ( oct.length() < 3 ) { oct.prepend( '0' ); }
    if ( code > 0x80 && code < 0x1000 ) { hex.prepend( ( code < 0x100 ? "00" : "0" ) ); }
    message( i18n("'%1' %2,  Hex %3,  Octal %4", ch, dec, hex, oct ) );
  }

  return true;
}

bool KateViNormalMode::commandRepeatLastChange()
{
  resetParser();
  doc()->editStart();
  m_viInputModeManager->repeatLastChange();
  doc()->editEnd();

  return true;
}

bool KateViNormalMode::commandAlignLine()
{
  const int line = m_view->cursorPosition().line();
  Range alignRange( Cursor(line, 0), Cursor(line, 0) );

  doc()->align( m_view, alignRange );

  return true;
}

bool KateViNormalMode::commandAlignLines()
{
  Cursor c( m_view->cursorPosition() );
  m_commandRange.normalize();

  Cursor start(m_commandRange.startLine, 0);
  Cursor end(m_commandRange.endLine, 0);

  doc()->align( m_view, Range( start, end ) );

  return true;
}

bool KateViNormalMode::commandAddToNumber()
{
    addToNumberUnderCursor( getCount() );

    return true;
}

bool KateViNormalMode::commandSubtractFromNumber()
{
    addToNumberUnderCursor( -getCount() );

    return true;
}

bool KateViNormalMode::commandPrependToBlock()
{
  Cursor c( m_view->cursorPosition() );

  // move cursor to top left corner of selection
  m_commandRange.normalize();
  c.setColumn( m_commandRange.startColumn );
  c.setLine( m_commandRange.startLine );
  updateCursor( c );

  m_stickyColumn = -1;
  m_viInputModeManager->getViInsertMode()->setBlockPrependMode( m_commandRange );
  return startInsertMode();
}

bool KateViNormalMode::commandAppendToBlock()
{
  Cursor c( m_view->cursorPosition() );

  m_commandRange.normalize();
  if ( m_stickyColumn == (unsigned int)KateVi::EOL ) { // append to EOL
    // move cursor to end of first line
    c.setLine( m_commandRange.startLine );
    c.setColumn( doc()->lineLength( c.line() ) );
    updateCursor( c );
    m_viInputModeManager->getViInsertMode()->setBlockAppendMode( m_commandRange, AppendEOL );
  } else {
    m_viInputModeManager->getViInsertMode()->setBlockAppendMode( m_commandRange, Append );
    // move cursor to top right corner of selection
    c.setColumn( m_commandRange.endColumn+1 );
    c.setLine( m_commandRange.startLine );
    updateCursor( c );
  }

  m_stickyColumn = -1;

  return startInsertMode();
}

bool KateViNormalMode::commandGoToNextJump() {
    Cursor c = getNextJump(m_view->cursorPosition());
    updateCursor(c);

    return true;
}

bool KateViNormalMode::commandGoToPrevJump() {
    Cursor c = getPrevJump(m_view->cursorPosition());
    updateCursor(c);

    return true;
}

bool KateViNormalMode::commandSwitchToLeftView() {
    switchView(Left);
    return true;
}

bool KateViNormalMode::commandSwitchToDownView() {
    switchView(Down);
    return true;
}

bool KateViNormalMode::commandSwitchToUpView() {
    switchView(Up);
    return true;
}

bool KateViNormalMode::commandSwitchToRightView() {
    switchView(Right);
    return true;
}

bool KateViNormalMode::commandSwitchToNextView() {
    switchView(Next);
    return true;
}

bool KateViNormalMode::commandSplitHoriz() {
  m_view->cmdLineBar()->execute("split");
  return true;
}

bool KateViNormalMode::commandSplitVert() {
  m_view->cmdLineBar()->execute("vsplit");
  return true;
}

bool KateViNormalMode::commandSwitchToNextTab() {
  QString command = "bn";

  if ( m_iscounted )
    command = command + " " + QString::number(getCount());

  m_view->cmdLineBar()->execute(command);
  return true;
}

bool KateViNormalMode::commandSwitchToPrevTab() {
  QString command = "bp";

  if ( m_iscounted )
    command = command + " " + QString::number(getCount());

  m_view->cmdLineBar()->execute(command);
  return true;
}

bool KateViNormalMode::commandFormatLine()
{
  Cursor c( m_view->cursorPosition() );

  reformatLines( c.line(), c.line()+getCount()-1 );

  return true;
}

bool KateViNormalMode::commandFormatLines()
{
  reformatLines( m_commandRange.startLine, m_commandRange.endLine );
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// MOTIONS
////////////////////////////////////////////////////////////////////////////////

KateViRange KateViNormalMode::motionDown()
{
  return goLineDown();
}

KateViRange KateViNormalMode::motionUp()
{
  return goLineUp();
}

KateViRange KateViNormalMode::motionLeft()
{
  Cursor cursor ( m_view->cursorPosition() );
  m_stickyColumn = -1;
  KateViRange r( cursor.line(), cursor.column(), ViMotion::ExclusiveMotion );
  r.endColumn -= getCount();

  if ( r.endColumn < 0 ) {
    r.endColumn = 0;
  }

  return r;
}

KateViRange KateViNormalMode::motionRight()
{
  Cursor cursor ( m_view->cursorPosition() );
  m_stickyColumn = -1;
  KateViRange r( cursor.line(), cursor.column(), ViMotion::ExclusiveMotion );
  r.endColumn += getCount();

  // make sure end position isn't > line length
  if ( r.endColumn > doc()->lineLength( r.endLine ) ) {
    r.endColumn = doc()->lineLength( r.endLine );
  }

  return r;
}

KateViRange KateViNormalMode::motionPageDown()
{
  Cursor c( m_view->cursorPosition() );
  int linesToScroll = linesDisplayed();

  KateViRange r( c.line()+linesToScroll, c.column(), ViMotion::InclusiveMotion );

  if ( r.endLine >= doc()->lines() ) {
    r.endLine = doc()->lines()-1;
  }

  return r;
}

KateViRange KateViNormalMode::motionPageUp()
{
  Cursor c( m_view->cursorPosition() );
  int linesToScroll = linesDisplayed();

  KateViRange r( c.line()-linesToScroll, c.column(), ViMotion::InclusiveMotion );

  if ( r.endLine < 0 ) {
    r.endLine = 0;
  }

  return r;
}

KateViRange KateViNormalMode::motionDownToFirstNonBlank()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r = goLineDown();

  r.endColumn = getLine( r.endLine ).indexOf( QRegExp( "\\S" ) );

  if ( r.endColumn < 0 ) {
    r.endColumn = 0;
  }

  return r;
}

KateViRange KateViNormalMode::motionUpToFirstNonBlank()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r = goLineUp();

  r.endColumn = getLine( r.endLine ).indexOf( QRegExp( "\\S" ) );

  if ( r.endColumn < 0 ) {
    r.endColumn = 0;
  }

  return r;
}

KateViRange KateViNormalMode::motionWordForward()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r( c.line(), c.column(), ViMotion::ExclusiveMotion );

  m_stickyColumn = -1;

  // Special case: If we're already on the very last character in the document, the motion should be
  // inclusive so the last character gets included
  if ( c.line() == doc()->lines()-1 && c.column() == doc()->lineLength( c.line() )-1 ) {
    r.motionType = ViMotion::InclusiveMotion;
  } else {
    for ( unsigned int i = 0; i < getCount(); i++ ) {
      c = findNextWordStart( c.line(), c.column() );

      // stop when at the last char in the document
      if ( c.line() == doc()->lines()-1 && c.column() == doc()->lineLength( c.line() )-1 ) {
        // if we still haven't "used up the count", make the motion inclusive, so that the last char
        // is included
        if ( i < getCount() ) {
          r.motionType = ViMotion::InclusiveMotion;
        }
        break;
      }
    }
  }

  r.endColumn = c.column();
  r.endLine = c.line();

  return r;
}

KateViRange KateViNormalMode::motionWordBackward()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r( c.line(), c.column(), ViMotion::ExclusiveMotion );

  m_stickyColumn = -1;

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    c = findPrevWordStart( c.line(), c.column() );

    // stop when at the first char in the document
    if ( c.line() == 0 && c.column() == 0 ) {
      break;
    }
  }

  r.endColumn = c.column();
  r.endLine = c.line();

  return r;
}

KateViRange KateViNormalMode::motionWORDForward()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r( c.line(), c.column(), ViMotion::ExclusiveMotion );

  m_stickyColumn = -1;

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    c = findNextWORDStart( c.line(), c.column() );

    // stop when at the last char in the document
    if ( c.line() == doc()->lines()-1 && c.column() == doc()->lineLength( c.line() )-1 ) {
      break;
    }
  }

  r.endColumn = c.column();
  r.endLine = c.line();

  return r;
}

KateViRange KateViNormalMode::motionWORDBackward()
{
  Cursor c( m_view->cursorPosition() );
  KateViRange r( c.line(), c.column(), ViMotion::ExclusiveMotion );

  m_stickyColumn = -1;

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    c = findPrevWORDStart( c.line(), c.column() );

    // stop when at the first char in the document
    if ( c.line() == 0 && c.column() == 0 ) {
      break;
    }
  }

  r.endColumn = c.column();
  r.endLine = c.line();

  return r;
}

KateViRange KateViNormalMode::motionToEndOfWord()
{
    Cursor c( m_view->cursorPosition() );
    KateViRange r( c.line(), c.column(), ViMotion::InclusiveMotion );

    m_stickyColumn = -1;

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c = findWordEnd( c.line(), c.column() );
    }

    r.endColumn = c.column();
    r.endLine = c.line();

    return r;
}

KateViRange KateViNormalMode::motionToEndOfWORD()
{
    Cursor c( m_view->cursorPosition() );
    KateViRange r( c.line(), c.column(), ViMotion::InclusiveMotion );

    m_stickyColumn = -1;

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c = findWORDEnd( c.line(), c.column() );
    }

    r.endColumn = c.column();
    r.endLine = c.line();

    return r;
}

KateViRange KateViNormalMode::motionToEndOfPrevWord()
{
    Cursor c( m_view->cursorPosition() );
    KateViRange r( c.line(), c.column(), ViMotion::InclusiveMotion );

    m_stickyColumn = -1;

    for ( unsigned int i = 0; i < getCount(); i++ ) {
      c = findPrevWordEnd( c.line(), c.column() );

      // stop when at the first char in the document
      if ( c.line() == 0 && c.column() == 0 ) {
        break;
      }
    }

    r.endColumn = c.column();
    r.endLine = c.line();

    return r;
}

KateViRange KateViNormalMode::motionToEndOfPrevWORD()
{
    Cursor c( m_view->cursorPosition() );
    KateViRange r( c.line(), c.column(), ViMotion::InclusiveMotion );

    m_stickyColumn = -1;

    for ( unsigned int i = 0; i < getCount(); i++ ) {
      c = findPrevWORDEnd( c.line(), c.column() );

      // stop when at the first char in the document
      if ( c.line() == 0 && c.column() == 0 ) {
        break;
      }
    }

    r.endColumn = c.column();
    r.endLine = c.line();

    return r;
}

KateViRange KateViNormalMode::motionToEOL()
{
  Cursor c( m_view->cursorPosition() );

  // set sticky column to a ridiculously high value so that the cursor will stick to EOL,
  // but only if it's a regular motion
  if ( m_keys.size() == 1 ) {
    m_stickyColumn = KateVi::EOL;
  }

  unsigned int line = c.line() + ( getCount() - 1 );
  KateViRange r( line, doc()->lineLength(line )-1, ViMotion::InclusiveMotion );

  return r;
}

KateViRange KateViNormalMode::motionToColumn0()
{
  m_stickyColumn = -1;
  Cursor cursor ( m_view->cursorPosition() );
  KateViRange r( cursor.line(), 0, ViMotion::ExclusiveMotion );

  return r;
}

KateViRange KateViNormalMode::motionToFirstCharacterOfLine()
{
  m_stickyColumn = -1;

  Cursor cursor ( m_view->cursorPosition() );
  QRegExp nonSpace( "\\S" );
  int c = getLine().indexOf( nonSpace );

  KateViRange r( cursor.line(), c, ViMotion::ExclusiveMotion );

  return r;
}

KateViRange KateViNormalMode::motionFindChar()
{
  m_lastTFcommand = m_keys;
  Cursor cursor ( m_view->cursorPosition() );
  QString line = getLine();

  m_stickyColumn = -1;

  int matchColumn = cursor.column();

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    matchColumn = line.indexOf( m_keys.right( 1 ), matchColumn+1 );
    if ( matchColumn == -1 )
      break;
  }

  KateViRange r;

  r.startColumn = cursor.column();
  r.startLine = cursor.line();
  r.endColumn = matchColumn;
  r.endLine = cursor.line();

  return r;
}

KateViRange KateViNormalMode::motionFindCharBackward()
{
  m_lastTFcommand = m_keys;
  Cursor cursor ( m_view->cursorPosition() );
  QString line = getLine();

  m_stickyColumn = -1;

  int matchColumn = -1;

  unsigned int hits = 0;
  int i = cursor.column()-1;

  while ( hits != getCount() && i >= 0 ) {
    if ( line.at( i ) == m_keys.at( m_keys.size()-1 ) )
      hits++;

    if ( hits == getCount() )
      matchColumn = i;

    i--;
  }

  KateViRange r;

  r.endColumn = matchColumn;
  r.endLine = cursor.line();

  return r;
}

KateViRange KateViNormalMode::motionToChar()
{
  m_lastTFcommand = m_keys;
  Cursor cursor ( m_view->cursorPosition() );
  QString line = getLine();

  m_stickyColumn = -1;

  int matchColumn = cursor.column()+1;

  for ( unsigned int i = 0; i < getCount(); i++ ) {
    matchColumn = line.indexOf( m_keys.right( 1 ), matchColumn );
    if ( matchColumn == -1 )
      break;
  }

  KateViRange r;

  r.endColumn = matchColumn-1;
  r.endLine = cursor.line();

  return r;
}

KateViRange KateViNormalMode::motionToCharBackward()
{
  m_lastTFcommand = m_keys;
  Cursor cursor ( m_view->cursorPosition() );
  QString line = getLine();

  m_stickyColumn = -1;

  int matchColumn = -1;

  unsigned int hits = 0;
  int i = cursor.column()-1;

  while ( hits != getCount() && i >= 0 ) {
    if ( line.at( i ) == m_keys.at( m_keys.size()-1 ) )
      hits++;

    if ( hits == getCount() )
      matchColumn = i;

    i--;
  }

  KateViRange r;

  r.endColumn = matchColumn+1;
  r.endLine = cursor.line();

  return r;
}

KateViRange KateViNormalMode::motionRepeatlastTF()
{
  if ( !m_lastTFcommand.isEmpty() ) {
    m_keys = m_lastTFcommand;
    if ( m_keys.at( 0 ) == 'f' ) {
      return motionFindChar();
    }
    else if ( m_keys.at( 0 ) == 'F' ) {
      return motionFindCharBackward();
    }
    else if ( m_keys.at( 0 ) == 't' ) {
      return motionToChar();
    }
    else if ( m_keys.at( 0 ) == 'T' ) {
      return motionToCharBackward();
    }
  }

  // there was no previous t/f command

  KateViRange r;
  r.valid = false;

  return r;
}

KateViRange KateViNormalMode::motionRepeatlastTFBackward()
{
  if ( !m_lastTFcommand.isEmpty() ) {
    m_keys = m_lastTFcommand;
    if ( m_keys.at( 0 ) == 'f' ) {
      return motionFindCharBackward();
    }
    else if ( m_keys.at( 0 ) == 'F' ) {
      return motionFindChar();
    }
    else if ( m_keys.at( 0 ) == 't' ) {
      return motionToCharBackward();
    }
    else if ( m_keys.at( 0 ) == 'T' ) {
      return motionToChar();
    }
  }

  // there was no previous t/f command

  KateViRange r;
  r.valid = false;

  return r;
}

// FIXME: should honour the provided count
KateViRange KateViNormalMode::motionFindPrev()
{
  QString pattern = m_viInputModeManager->getLastSearchPattern();
  bool backwards = m_viInputModeManager->lastSearchBackwards();

  return findPattern( pattern, !backwards, getCount() );
}

KateViRange KateViNormalMode::motionFindNext()
{
  QString pattern = m_viInputModeManager->getLastSearchPattern();
  bool backwards = m_viInputModeManager->lastSearchBackwards();

  return findPattern( pattern, backwards, getCount() );
}


KateViRange KateViNormalMode::motionToLineFirst()
{
    KateViRange r( getCount()-1, 0, ViMotion::InclusiveMotion );
    m_stickyColumn = -1;

    if ( r.endLine > doc()->lines() - 1 ) {
      r.endLine = doc()->lines() - 1;
    }
    r.jump = true;

    return r;
}

KateViRange KateViNormalMode::motionToLineLast()
{
  KateViRange r( doc()->lines()-1, 0, ViMotion::InclusiveMotion );
  m_stickyColumn = -1;

  // don't use getCount() here, no count and a count of 1 is different here...
  if ( m_count != 0 ) {
    r.endLine = m_count-1;
  }

  if ( r.endLine > doc()->lines() - 1 ) {
    r.endLine = doc()->lines() - 1;
  }
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToScreenColumn()
{
  m_stickyColumn = -1;

  Cursor c( m_view->cursorPosition() );

  int column = getCount()-1;

  if ( doc()->lineLength( c.line() )-1 < (int)getCount()-1 ) {
      column = doc()->lineLength( c.line() )-1;
  }

  return KateViRange( c.line(), column, ViMotion::ExclusiveMotion );
}

KateViRange KateViNormalMode::motionToMark()
{
  KateViRange r;

  m_stickyColumn = -1;

  QChar reg = m_keys.at( m_keys.size()-1 );

  // ` and ' is the same register (position before jump)
  if ( reg == '`' ) {
      reg = '\'';
  }

  Cursor c = m_view->getViInputModeManager()->getMarkPosition( reg );
  if ( c.isValid() ) {
    r.endLine = c.line();
    r.endColumn = c.column();
  } else {
    error(i18n("Mark not set: %1",m_keys.right( 1 ) ));
    r.valid = false;
  }

  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToMarkLine()
{
  KateViRange r = motionToMark();
  r.endColumn = 0; // FIXME: should be first non-blank on line

  m_stickyColumn = -1;

  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToMatchingItem()
{
  KateViRange r;
  int lines = doc()->lines();

  // If counted then it is not a motion to matching item anymore
  // it's a motions to the N'th % of the document
  if(isCounted()) {
    if (getCount() > 100)
        return r;

    r.endLine = qRound(lines * getCount() / 100.) -1 ;
    r.endColumn = 0;
    return r;
  }

  Cursor c( m_view->cursorPosition() );

  r.startColumn = c.column();
  r.startLine   = c.line();

  QString l = getLine();
  int n1 = l.indexOf( m_matchItemRegex, c.column() );
  int n2;

  m_stickyColumn = -1;

  if ( n1 == -1 ) {
    r.valid = false;
    return r;
  } else {
    n2 = l.indexOf( QRegExp( "\\b|\\s|$" ), n1 );
  }

  // text item we want to find a matching item for
  QString item = l.mid( n1, n2-n1 );

  // use kate's built-in matching bracket finder for brackets
  if ( QString("{}()[]").indexOf( item ) != -1 ) {

    // move the cursor to the first bracket
    c.setColumn( n1+1 );
    updateCursor( c );

    // find the matching one
    c = m_viewInternal->findMatchingBracket();

    if ( c > m_view->cursorPosition() ) {
      c.setColumn( c.column()-1 );
    }
  } else {
    int toFind = 1;
    //int n2 = l.indexOf( QRegExp( "\\s|$" ), n1 );

    //QString item = l.mid( n1, n2-n1 );
    QString matchingItem = m_matchingItems[ item ];

    int line = c.line();
    int column = n2-item.length();
    bool reverse = false;

    if ( matchingItem.left( 1 ) == "-" ) {
      matchingItem.remove( 0, 1 ); // remove the '-'
      reverse = true;
    }

    // make sure we don't hit the text item we start the search from
    if ( column == 0 && reverse ) {
      column -= item.length();
    }

    int itemIdx;
    int matchItemIdx;

    while ( toFind > 0 ) {
      if ( !reverse ) {
        itemIdx = l.indexOf( item, column );
        matchItemIdx = l.indexOf( matchingItem, column );

        if ( itemIdx != -1 && ( matchItemIdx == -1 || itemIdx < matchItemIdx ) ) {
            toFind++;
        }
      } else {
        itemIdx = l.lastIndexOf( item, column-1 );
        matchItemIdx = l.lastIndexOf( matchingItem, column-1 );

        if ( itemIdx != -1 && ( matchItemIdx == -1 || itemIdx > matchItemIdx ) ) {
            toFind++;
        }
      }

      if ( matchItemIdx != -1 || itemIdx != -1 ) {
          if ( !reverse ) {
            column = qMin( (unsigned int)itemIdx, (unsigned int)matchItemIdx );
          } else {
            column = qMax( itemIdx, matchItemIdx );
          }
      }

      if ( matchItemIdx != -1 ) { // match on current line
          if ( matchItemIdx == column ) {
              toFind--;
              c.setLine( line );
              c.setColumn( column );
          }
      } else { // no match, advance one line if possible
        ( reverse) ? line-- : line++;
        column = 0;

        if ( ( !reverse && line >= lines ) || ( reverse && line < 0 ) ) {
          r.valid = false;
          break;
        } else {
          l = getLine( line );
        }
      }
    }
  }

  r.endLine = c.line();
  r.endColumn = c.column();
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToNextBraceBlockStart()
{
  KateViRange r;

  m_stickyColumn = -1;

  int line = findLineStartingWitchChar( '{', getCount() );

  if ( line == -1 ) {
    r.valid = false;
    return r;
  }

  r.endLine = line;
  r.endColumn = 0;
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToPreviousBraceBlockStart()
{
  KateViRange r;

  m_stickyColumn = -1;

  int line = findLineStartingWitchChar( '{', getCount(), false );

  if ( line == -1 ) {
    r.valid = false;
    return r;
  }

  r.endLine = line;
  r.endColumn = 0;
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToNextBraceBlockEnd()
{
  KateViRange r;

  m_stickyColumn = -1;

  int line = findLineStartingWitchChar( '}', getCount() );

  if ( line == -1 ) {
    r.valid = false;
    return r;
  }

  r.endLine = line;
  r.endColumn = 0;
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToPreviousBraceBlockEnd()
{
  KateViRange r;

  m_stickyColumn = -1;

  int line = findLineStartingWitchChar( '{', getCount(), false );

  if ( line == -1 ) {
    r.valid = false;
    return r;
  }

  r.endLine = line;
  r.endColumn = 0;
  r.jump = true;

  return r;
}

KateViRange KateViNormalMode::motionToNextOccurrence()
{
  QString word = getWordUnderCursor();
  word.prepend("\\b").append("\\b");

  m_viInputModeManager->setLastSearchPattern( word );
  m_viInputModeManager->setLastSearchBackwards( false );

  return findPattern( word, false, getCount() );
}

KateViRange KateViNormalMode::motionToPrevOccurrence()
{
  QString word = getWordUnderCursor();
  word.prepend("\\b").append("\\b");

  m_viInputModeManager->setLastSearchPattern( word );
  m_viInputModeManager->setLastSearchBackwards( true );

  return findPattern( word, true, getCount() );
}

KateViRange KateViNormalMode::motionToFirstLineOfWindow() {
    int lines_to_go;
    if (linesDisplayed() <= (unsigned int) m_viewInternal->endLine())
       lines_to_go = m_viewInternal->endLine() - linesDisplayed()- m_view->cursorPosition().line() + 1;
    else
        lines_to_go = - m_view->cursorPosition().line();

    KateViRange r = goLineUpDown(lines_to_go);

    // Finding first non-blank character
    QRegExp nonSpace( "\\S" );
    int c = getLine(r.endLine).indexOf( nonSpace );
    if ( c == -1 )
      c = 0;

    r.endColumn = c;
    return r;
}

KateViRange KateViNormalMode::motionToMiddleLineOfWindow() {
    int lines_to_go;
    if (linesDisplayed() <= (unsigned int) m_viewInternal->endLine())
       lines_to_go = m_viewInternal->endLine() - linesDisplayed()/2 - m_view->cursorPosition().line();
    else
        lines_to_go = m_viewInternal->endLine()/2 - m_view->cursorPosition().line();
    KateViRange r = goLineUpDown(lines_to_go);

    // Finding first non-blank character
    QRegExp nonSpace( "\\S" );
    int c = getLine(r.endLine).indexOf( nonSpace );
    if ( c == -1 )
      c = 0;

    r.endColumn = c;
    return r;
}

KateViRange KateViNormalMode::motionToLastLineOfWindow() {
    int lines_to_go;
    if (linesDisplayed() <= (unsigned int) m_viewInternal->endLine())
       lines_to_go = m_viewInternal->endLine() - m_view->cursorPosition().line();
    else
        lines_to_go = m_viewInternal->endLine() - m_view->cursorPosition().line();

    KateViRange r = goLineUpDown(lines_to_go);

    // Finding first non-blank character
    QRegExp nonSpace( "\\S" );
    int c = getLine(r.endLine).indexOf( nonSpace );
    if ( c == -1 )
      c = 0;

    r.endColumn = c;
    return r;
}

KateViRange KateViNormalMode::motionToNextVisualLine() {
  return goVisualLineUpDown( getCount() );
}

KateViRange KateViNormalMode::motionToPrevVisualLine() {
  return goVisualLineUpDown( -getCount() );
}

////////////////////////////////////////////////////////////////////////////////
// TEXT OBJECTS
////////////////////////////////////////////////////////////////////////////////

KateViRange KateViNormalMode::textObjectAWord()
{
    Cursor c( m_view->cursorPosition() );

    Cursor c1 = findPrevWordStart( c.line(), c.column()+1, true );
    Cursor c2( c );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c2 = findNextWordStart( c2.line(), c2.column(), true );
    }

    c2.setColumn( c2.column()-1 ); // don't include the first char of the following word
    KateViRange r( c.line(), c.column(), ViMotion::InclusiveMotion );

    // sanity check
    if ( c1.line() != c2.line() || c1.column() > c2.column() ) {
        r.valid = false;
    } else {
        r.startLine = c1.line();
        r.endLine = c2.line();
        r.startColumn = c1.column();
        r.endColumn = c2.column();
    }

    return r;
}

KateViRange KateViNormalMode::textObjectInnerWord()
{
    Cursor c( m_view->cursorPosition() );

    Cursor c1 = findPrevWordStart( c.line(), c.column()+1, true );
    // need to start search in column-1 because it might be a one-character word
    Cursor c2( c.line(), c.column()-1 );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c2 = findWordEnd( c2.line(), c2.column(), true );
    }

    KateViRange r;

    // sanity check
    if ( c1.line() != c2.line() || c1.column() > c2.column() ) {
        r.valid = false;
    } else {
        r.startLine = c1.line();
        r.endLine = c2.line();
        r.startColumn = c1.column();
        r.endColumn = c2.column();
    }

    return r;
}

KateViRange KateViNormalMode::textObjectAWORD()
{
    Cursor c( m_view->cursorPosition() );

    Cursor c1 = findPrevWORDStart( c.line(), c.column()+1, true );
    Cursor c2( c );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c2 = findNextWORDStart( c2.line(), c2.column(), true );
    }

    KateViRange r( c.line(), c.column(), ViMotion::ExclusiveMotion );

    // sanity check
    if ( c1.line() != c2.line() || c1.column() > c2.column() ) {
        r.valid = false;
    } else {
        r.startLine = c1.line();
        r.endLine = c2.line();
        r.startColumn = c1.column();
        r.endColumn = c2.column();
    }

    return r;
}

KateViRange KateViNormalMode::textObjectInnerWORD()
{
    Cursor c( m_view->cursorPosition() );

    Cursor c1 = findPrevWORDStart( c.line(), c.column()+1, true );
    Cursor c2( c );

    for ( unsigned int i = 0; i < getCount(); i++ ) {
        c2 = findWORDEnd( c2.line(), c2.column(), true );
    }

    KateViRange r;

    // sanity check
    if ( c1.line() != c2.line() || c1.column() > c2.column() ) {
        r.valid = false;
    } else {
        r.startLine = c1.line();
        r.endLine = c2.line();
        r.startColumn = c1.column();
        r.endColumn = c2.column();
    }

    return r;
}

KateViRange KateViNormalMode::textObjectAQuoteDouble()
{
    return findSurroundingQuotes( '"', false );
}

KateViRange KateViNormalMode::textObjectInnerQuoteDouble()
{
    return findSurroundingQuotes( '"', true );
}

KateViRange KateViNormalMode::textObjectAQuoteSingle()
{
    return findSurroundingQuotes( '\'', false );
}

KateViRange KateViNormalMode::textObjectInnerQuoteSingle()
{
    return findSurroundingQuotes( '\'', true );
}

KateViRange KateViNormalMode::textObjectABackQuote()
{
    return findSurroundingQuotes( '`', false );
}

KateViRange KateViNormalMode::textObjectInnerBackQuote()
{
    return findSurroundingQuotes( '`', true );
}


KateViRange KateViNormalMode::textObjectAParen()
{

    return findSurroundingBrackets( '(', ')', false,  '(', ')' );
}

KateViRange KateViNormalMode::textObjectInnerParen()
{

    return findSurroundingBrackets( '(', ')', true, '(', ')');
}

KateViRange KateViNormalMode::textObjectABracket()
{

    return findSurroundingBrackets( '[', ']', false,  '[', ']' );
}

KateViRange KateViNormalMode::textObjectInnerBracket()
{

    return findSurroundingBrackets( '[', ']', true, '[', ']' );
}

KateViRange KateViNormalMode::textObjectACurlyBracket()
{

    return findSurroundingBrackets( '{', '}', false,  '{', '}' );
}

KateViRange KateViNormalMode::textObjectInnerCurlyBracket()
{

    return findSurroundingBrackets( '{', '}', true, '{', '}' );
}

KateViRange KateViNormalMode::textObjectAInequalitySign()
{

    return findSurroundingBrackets( '<', '>', false,  '<', '>' );
}

KateViRange KateViNormalMode::textObjectInnerInequalitySign()
{

    return findSurroundingBrackets( '<', '>', true, '<', '>' );
}

KateViRange KateViNormalMode::textObjectAComma()
{
  KateViRange r = findSurroundingQuotes( ',', false );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', ')', false, '(', ')' );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', ']', false, '[', ']' );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', '}', false, '{', '}' );

  if ( !r.valid )
    r = findSurroundingBrackets( '(', ',', false, '(', ')'  );

  if ( !r.valid )
    r = findSurroundingBrackets( '[', ',', false, '[', ']' );

  if ( !r.valid )
    r = findSurroundingBrackets( '{', ',', false, '{', '}' );

    return r;
}

KateViRange KateViNormalMode::textObjectInnerComma()
{

  KateViRange r = findSurroundingQuotes( ',',true );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', ')', true, '(', ')' );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', ']', true, '[', ']' );

  if ( !r.valid )
    r = findSurroundingBrackets( ',', '}', true, '{', '}' );

  if ( !r.valid )
    r = findSurroundingBrackets( '(', ',', true, '(', ')'  );

  if ( !r.valid )
    r = findSurroundingBrackets( '[', ',', true, '[', ']' );

  if ( !r.valid )
    r = findSurroundingBrackets( '{', ',', true, '{', '}' );

    return r;
}

// add commands
// when adding commands here, remember to add them to visual mode too (if applicable)
void KateViNormalMode::initializeCommands()
{
  ADDCMD("a", commandEnterInsertModeAppend, IS_CHANGE );
  ADDCMD("A", commandEnterInsertModeAppendEOL, IS_CHANGE );
  ADDCMD("i", commandEnterInsertMode, IS_CHANGE );
  ADDCMD("I", commandEnterInsertModeBeforeFirstNonBlankInLine, IS_CHANGE );
  ADDCMD("v", commandEnterVisualMode, 0 );
  ADDCMD("V", commandEnterVisualLineMode, 0 );
  ADDCMD("<c-v>", commandEnterVisualBlockMode, 0 );
  ADDCMD("gv", commandReselectVisual, 0 );
  ADDCMD("o", commandOpenNewLineUnder, IS_CHANGE );
  ADDCMD("O", commandOpenNewLineOver, IS_CHANGE );
  ADDCMD("J", commandJoinLines, IS_CHANGE );
  ADDCMD("c", commandChange, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("C", commandChangeToEOL, IS_CHANGE );
  ADDCMD("cc", commandChangeLine, IS_CHANGE );
  ADDCMD("s", commandSubstituteChar, IS_CHANGE );
  ADDCMD("S", commandSubstituteLine, IS_CHANGE );
  ADDCMD("dd", commandDeleteLine, IS_CHANGE );
  ADDCMD("d", commandDelete, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("D", commandDeleteToEOL, IS_CHANGE );
  ADDCMD("x", commandDeleteChar, IS_CHANGE );
  ADDCMD("X", commandDeleteCharBackward, IS_CHANGE );
  ADDCMD("gu", commandMakeLowercase, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("guu", commandMakeLowercaseLine, IS_CHANGE );
  ADDCMD("gU", commandMakeUppercase, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("gUU", commandMakeUppercaseLine, IS_CHANGE );
  ADDCMD("y", commandYank, NEEDS_MOTION );
  ADDCMD("yy", commandYankLine, 0 );
  ADDCMD("Y", commandYankToEOL, 0 );
  ADDCMD("p", commandPaste, IS_CHANGE );
  ADDCMD("P", commandPasteBefore, IS_CHANGE );
  ADDCMD("r.", commandReplaceCharacter, IS_CHANGE | REGEX_PATTERN );
  ADDCMD("R", commandEnterReplaceMode, IS_CHANGE );
  ADDCMD(":", commandSwitchToCmdLine, 0 );
  ADDCMD("/", commandSearch, 0 );
  ADDCMD("u", commandUndo, 0);
  ADDCMD("<c-r>", commandRedo, 0 );
  ADDCMD("U", commandRedo, 0 );
  ADDCMD("m.", commandSetMark, REGEX_PATTERN );
  ADDCMD(">>", commandIndentLine, IS_CHANGE );
  ADDCMD("<<", commandUnindentLine, IS_CHANGE );
  ADDCMD(">", commandIndentLines, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("<", commandUnindentLines, IS_CHANGE | NEEDS_MOTION );
  ADDCMD("<c-f>", commandScrollPageDown, 0 );
  ADDCMD("<pagedown>", commandScrollPageDown, 0 );
  ADDCMD("<c-b>", commandScrollPageUp, 0 );
  ADDCMD("<pageup>", commandScrollPageUp, 0 );
  ADDCMD("<c-u>", commandScrollHalfPageUp, 0 );
  ADDCMD("<c-d>", commandScrollHalfPageDown, 0 );
  ADDCMD("zz", commandCentreViewOnCursor, 0 );
  ADDCMD("ga", commandPrintCharacterCode, SHOULD_NOT_RESET );
  ADDCMD(".", commandRepeatLastChange, 0 );
  ADDCMD("==", commandAlignLine, IS_CHANGE );
  ADDCMD("=", commandAlignLines, IS_CHANGE | NEEDS_MOTION);
  ADDCMD("~", commandChangeCase, IS_CHANGE );
  ADDCMD("<c-a>", commandAddToNumber, IS_CHANGE );
  ADDCMD("<c-x>", commandSubtractFromNumber, IS_CHANGE );
  ADDCMD("<c-o>", commandGoToPrevJump, 0);
  ADDCMD("<c-i>", commandGoToNextJump, 0);

  ADDCMD("<c-w>h",commandSwitchToLeftView,0);
  ADDCMD("<c-w><c-h>",commandSwitchToLeftView,0);
  ADDCMD("<c-w><left>",commandSwitchToLeftView,0);
  ADDCMD("<c-w>j",commandSwitchToDownView,0);
  ADDCMD("<c-w><c-j>",commandSwitchToDownView,0);
  ADDCMD("<c-w><down>",commandSwitchToDownView,0);
  ADDCMD("<c-w>k",commandSwitchToUpView,0);
  ADDCMD("<c-w><c-k>",commandSwitchToUpView,0);
  ADDCMD("<c-w><up>",commandSwitchToUpView,0);
  ADDCMD("<c-w>l",commandSwitchToRightView,0);
  ADDCMD("<c-w><c-l>",commandSwitchToRightView,0);
  ADDCMD("<c-w><right>",commandSwitchToRightView,0);
  ADDCMD("<c-w>w",commandSwitchToNextView,0);
  ADDCMD("<c-w><c-w>",commandSwitchToNextView,0);

  ADDCMD("<c-w>s", commandSplitHoriz, 0);
  ADDCMD("<c-w>S", commandSplitHoriz, 0);
  ADDCMD("<c-w><c-s>", commandSplitHoriz, 0);
  ADDCMD("<c-w>v", commandSplitVert, 0);
  ADDCMD("<c-w><c-v>", commandSplitVert, 0);

  ADDCMD("gt", commandSwitchToNextTab,0);
  ADDCMD("gT", commandSwitchToPrevTab,0);

  ADDCMD("gqq", commandFormatLine, IS_CHANGE);
  ADDCMD("gq", commandFormatLines, IS_CHANGE | NEEDS_MOTION);


  // regular motions
  ADDMOTION("h", motionLeft, 0 );
  ADDMOTION("<left>", motionLeft, 0 );
  ADDMOTION("<backspace>", motionLeft, 0 );
  ADDMOTION("j", motionDown, 0 );
  ADDMOTION("<down>", motionDown, 0 );
  ADDMOTION("<enter>", motionDownToFirstNonBlank, 0 );
  ADDMOTION("k", motionUp, 0 );
  ADDMOTION("<up>", motionUp, 0 );
  ADDMOTION("-", motionUpToFirstNonBlank, 0 );
  ADDMOTION("l", motionRight, 0 );
  ADDMOTION("<right>", motionRight, 0 );
  ADDMOTION(" ", motionRight, 0 );
  ADDMOTION("$", motionToEOL, 0 );
  ADDMOTION("<end>", motionToEOL, 0 );
  ADDMOTION("0", motionToColumn0, 0 );
  ADDMOTION("<home>", motionToColumn0, 0 );
  ADDMOTION("^", motionToFirstCharacterOfLine, 0 );
  ADDMOTION("f.", motionFindChar, REGEX_PATTERN );
  ADDMOTION("F.", motionFindCharBackward, REGEX_PATTERN );
  ADDMOTION("t.", motionToChar, REGEX_PATTERN );
  ADDMOTION("T.", motionToCharBackward, REGEX_PATTERN );
  ADDMOTION(";", motionRepeatlastTF, 0 );
  ADDMOTION(",", motionRepeatlastTFBackward, 0 );
  ADDMOTION("n", motionFindNext, 0 );
  ADDMOTION("N", motionFindPrev, 0 );
  ADDMOTION("gg", motionToLineFirst, 0 );
  ADDMOTION("G", motionToLineLast, 0 );
  ADDMOTION("w", motionWordForward, IS_NOT_LINEWISE );
  ADDMOTION("W", motionWORDForward, IS_NOT_LINEWISE );
  ADDMOTION("b", motionWordBackward, 0 );
  ADDMOTION("B", motionWORDBackward, 0 );
  ADDMOTION("e", motionToEndOfWord, 0 );
  ADDMOTION("E", motionToEndOfWORD, 0 );
  ADDMOTION("ge", motionToEndOfPrevWord, 0 );
  ADDMOTION("gE", motionToEndOfPrevWORD, 0 );
  ADDMOTION("|", motionToScreenColumn, 0 );
  ADDMOTION("%", motionToMatchingItem, IS_NOT_LINEWISE );
  ADDMOTION("`[a-zA-Z><]", motionToMark, REGEX_PATTERN );
  ADDMOTION("'[a-zA-Z><]", motionToMarkLine, REGEX_PATTERN );
  ADDMOTION("[[", motionToPreviousBraceBlockStart, 0 );
  ADDMOTION("]]", motionToNextBraceBlockStart, 0 );
  ADDMOTION("[]", motionToPreviousBraceBlockEnd, 0 );
  ADDMOTION("][", motionToNextBraceBlockEnd, 0 );
  ADDMOTION("*", motionToNextOccurrence, 0 );
  ADDMOTION("#", motionToPrevOccurrence, 0 );
  ADDMOTION("H", motionToFirstLineOfWindow, 0 );
  ADDMOTION("M", motionToMiddleLineOfWindow, 0 );
  ADDMOTION("L", motionToLastLineOfWindow, 0 );
  ADDMOTION("gj", motionToNextVisualLine, 0 );
  ADDMOTION("gk", motionToPrevVisualLine, 0 );

  // text objects
  ADDMOTION("iw", textObjectInnerWord, 0 );
  ADDMOTION("aw", textObjectAWord, 0 );
  ADDMOTION("i\"", textObjectInnerQuoteDouble, IS_NOT_LINEWISE );
  ADDMOTION("a\"", textObjectAQuoteDouble, IS_NOT_LINEWISE );
  ADDMOTION("i'", textObjectInnerQuoteSingle, IS_NOT_LINEWISE );
  ADDMOTION("a'", textObjectAQuoteSingle, IS_NOT_LINEWISE );
  ADDMOTION("i`", textObjectInnerBackQuote, IS_NOT_LINEWISE );
  ADDMOTION("a`", textObjectABackQuote, IS_NOT_LINEWISE );
  ADDMOTION("i[()b]", textObjectInnerParen, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("a[()b]", textObjectAParen, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("i[{}B]", textObjectInnerCurlyBracket, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("a[{}B]", textObjectACurlyBracket, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("i[><]", textObjectInnerInequalitySign, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("a[><]", textObjectAInequalitySign, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("i[\\[\\]]", textObjectInnerBracket, REGEX_PATTERN | IS_NOT_LINEWISE);
  ADDMOTION("a[\\[\\]]", textObjectABracket, REGEX_PATTERN  | IS_NOT_LINEWISE);
  ADDMOTION("i,", textObjectInnerComma, IS_NOT_LINEWISE );
  ADDMOTION("a,", textObjectAComma, IS_NOT_LINEWISE);
}

QRegExp KateViNormalMode::generateMatchingItemRegex()
{
  QString pattern("\\[|\\]|\\{|\\}|\\(|\\)|");
  QList<QString> keys = m_matchingItems.keys();

  for ( int i = 0; i < keys.size(); i++ ) {
    QString s = m_matchingItems[ keys[ i ] ];
    s = s.replace( QRegExp( "^-" ), QChar() );
    s = s.replace( QRegExp( "\\*" ), "\\*" );
    s = s.replace( QRegExp( "\\+" ), "\\+" );
    s = s.replace( QRegExp( "\\[" ), "\\[" );
    s = s.replace( QRegExp( "\\]" ), "\\]" );
    s = s.replace( QRegExp( "\\(" ), "\\(" );
    s = s.replace( QRegExp( "\\)" ), "\\)" );
    s = s.replace( QRegExp( "\\{" ), "\\{" );
    s = s.replace( QRegExp( "\\}" ), "\\}" );

    pattern.append( s );

    if ( i != keys.size()-1 ) {
      pattern.append( '|' );
    }
  }

  return QRegExp( pattern );
}

void KateViNormalMode::addMapping( const QString &from, const QString &to )
{
    KateGlobal::self()->viInputModeGlobal()->addMapping( NormalMode, from, to );
}

const QString KateViNormalMode::getMapping( const QString &from ) const
{
    return KateGlobal::self()->viInputModeGlobal()->getMapping( NormalMode, from );
}

const QStringList KateViNormalMode::getMappings() const
{
    return KateGlobal::self()->viInputModeGlobal()->getMappings( NormalMode );
}

// returns the operation mode that should be used. this is decided by using the following heuristic:
// 1. if we're in visual block mode, it should be Block
// 2. if we're in visual line mode OR the range spans several lines, it should be LineWise
// 3. if neither of these is true, CharWise is returned
// 4. there are some motion that makes all operator charwise, if we have one of them mode will be CharWise
OperationMode KateViNormalMode::getOperationMode() const
{
  OperationMode m = CharWise;

  if ( m_viInputModeManager->getCurrentViMode() == VisualBlockMode ) {
    m = Block;
  } else if ( m_viInputModeManager->getCurrentViMode() == VisualLineMode
    || ( m_commandRange.startLine != m_commandRange.endLine
      && m_viInputModeManager->getCurrentViMode() != VisualMode )) {
    m = LineWise;
  }

  if ( m_commandWithMotion && !m_linewiseCommand )
        m = CharWise;

  return m;
}

void KateViNormalMode::joinLines(unsigned int from, unsigned int to) const
{
  // make sure we don't try to join lines past the document end
  if ( to >= (unsigned int)(doc()->lines()) ) {
    to = doc()->lines()-1;
  }

  // joining one line is a no-op
  if ( from == to ) return;

  doc()->joinLines( from, to );
}

void KateViNormalMode::reformatLines(unsigned int from, unsigned int to) const
{
  joinLines( from, to );
  doc()->wrapText( from, to );
}
