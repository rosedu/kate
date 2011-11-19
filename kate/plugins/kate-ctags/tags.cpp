/***************************************************************************
 *   Copyright (C) 2004 by Jens Dagerbo                                    *
 *   jens.dagerbo@swipnet.se                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "tags.h"
#include <stdio.h>

namespace ctags
{
#include "readtags.h"
}

#include "ctagskinds.h"

QString Tags::_tagsfile;

Tags::TagEntry::TagEntry() {}

Tags::TagEntry::TagEntry( const QString & tag, const QString & type, const QString & file, const QString & pattern )
	: tag(tag), type(type), file(file), pattern(pattern)
{}


bool Tags::hasTag( const QString & tag )
{
	ctags::tagFileInfo info;
	ctags::tagFile * file = ctags::tagsOpen( _tagsfile.toLocal8Bit(), &info );
	ctags::tagEntry entry;

	bool found = ( ctags::tagsFind( file, &entry, tag.toLocal8Bit(), TAG_FULLMATCH | TAG_OBSERVECASE ) == ctags::TagSuccess );

	ctags::tagsClose( file );

	return found;
}

unsigned int Tags::numberOfMatches( const QString & tagpart, bool partial )
{
	unsigned int n = 0;

	if ( tagpart.isEmpty() ) return 0;

	ctags::tagFileInfo info;
	ctags::tagFile * file = ctags::tagsOpen( _tagsfile.toLocal8Bit(), &info );
	ctags::tagEntry entry;

	if ( ctags::tagsFind( file, &entry, tagpart.toLocal8Bit(), TAG_OBSERVECASE | (partial ? TAG_PARTIALMATCH : TAG_FULLMATCH) ) == ctags::TagSuccess )
	{
		do
		{
			n++;
		}
		while ( ctags::tagsFindNext( file, &entry ) == ctags::TagSuccess );
	}

	ctags::tagsClose( file );

	return n;
}

Tags::TagList Tags::getMatches( const QString & tagpart, bool partial, const QStringList & types )
{
	Tags::TagList list;

	if ( tagpart.isEmpty() ) return list;

	ctags::tagFileInfo info;
	ctags::tagFile * file = ctags::tagsOpen( _tagsfile.toLocal8Bit(), &info );
	ctags::tagEntry entry;
	char tagName[80];

	sprintf(tagName, "%s", tagpart.toLocal8Bit().data());

	if ( ctags::tagsFind( file, &entry, tagName, TAG_OBSERVECASE | (partial ? TAG_PARTIALMATCH : TAG_FULLMATCH) ) == ctags::TagSuccess )
	{
		do
		{
			QString type( CTagsKinds::findKind( entry.kind, QString( entry.file ).section( '.', -1 ) ) );
			QString file( entry.file );

			if ( type.isEmpty() && file.endsWith( "Makefile" ) )
			{
				type = "macro";
			}
			if ( types.isEmpty() || types.contains( entry.kind ) )
			{
				list << TagEntry( QString( entry.name ), type, file, QString( entry.address.pattern ) );
			}
		}
		while ( ctags::tagsFindNext( file, &entry ) == ctags::TagSuccess );
	}

	ctags::tagsClose( file );

	return list;
}

void Tags::setTagsFile( const QString & file )
{
	_tagsfile = file.toLocal8Bit();
}

QString Tags::getTagsFile( )
{
	return _tagsfile;
}

unsigned int Tags::numberOfPartialMatches( const QString & tagpart )
{
	return numberOfMatches( tagpart, true );
}

unsigned int Tags::numberOfExactMatches( const QString & tagpart )
{
	return numberOfMatches( tagpart, false );
}

Tags::TagList Tags::getPartialMatches( const QString & tagpart )
{
	return getMatches( tagpart, true );
}

Tags::TagList Tags::getExactMatches( const QString & tag )
{
	return getMatches( tag, false );
}

Tags::TagList Tags::getPartialMatches( const QString & file, const QString & tagpart )
{
	setTagsFile( file );
	return getMatches( tagpart, true );
}

Tags::TagList Tags::getExactMatches( const QString & file, const QString & tag )
{
	setTagsFile( file );
	return getMatches( tag, false );
}

Tags::TagList Tags::getMatches( const QString & file, const QString & tagpart,  bool partial, const QStringList & types )
{
	setTagsFile( file );
	return getMatches( tagpart, partial, types);
}

	// kate: space-indent off; indent-width 4; tab-width 4; show-tabs off;


