/**************************************************************************
 *  Karlyriceditor - a lyrics editor for Karaoke songs                    *
 *  Copyright (C) 2009-2011 George Yunaev, support@karlyriceditor.com     *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *																	      *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#ifndef LYRICSEVENTS_H
#define LYRICSEVENTS_H

#include <QMap>
#include <QImage>

class Background;

class LyricsEvents
{
	public:
		LyricsEvents();
		~LyricsEvents();

		// copy
		LyricsEvents( const LyricsEvents& );

		// check
		bool isEmpty() const;

		// Add the events
		bool addEvent( qint64 timing, const QString& text );

		// When playing
		bool prepare( QString * errmsg = 0 );
		bool updated( qint64 timing ) const;
		void draw( qint64 timing, QImage& image );

		static QString validateEvent( const QString& text );

	private:
		class Event
		{
			public:
				qint64		timing;
				int			type;
				QString		data;
		};

		void	cleanPrepared();
		static bool parseEvent( const QString& text, Event * event = 0, QString * errmsg = 0 );

		// Event storage which is copied
		QMap< qint64, Event > m_events;

		// Prepared event storage which is NOT copied
		QMap< qint64, Background* > m_preparedEvents;

		// State
		QImage		m_cachedImage;
		qint64		m_lastUpdate;
		qint64		m_nextUpdate;

		// Current event timing
		qint64		m_eventTiming;
};

#endif // LYRICSEVENTS_H