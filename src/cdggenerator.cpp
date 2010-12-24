/**************************************************************************
 *  Karlyriceditor - a lyrics editor for Karaoke songs                    *
 *  Copyright (C) 2009 George Yunaev, support@karlyriceditor.com          *
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

#include <QMap>
#include <QList>
#include <QApplication>
#include <QPainter>
#include <QProgressDialog>
#include <QTextDocument>

#include "lyricsrenderer.h"
#include "cdggenerator.h"


// Color code indexes
static int COLOR_IDX_BACKGROUND = 0;	// background

CDGGenerator::CDGGenerator()
{
}

void CDGGenerator::init( const QColor& bgcolor, const QColor& titlecolor,
						 const QColor& actcolor, const QColor& inactcolor, const QFont& font )
{
	// Disable anti-aliasing for fonts
	m_renderFont = font;
	m_renderFont.setStyleStrategy(QFont::NoAntialias);
	m_renderFont.setWeight( QFont::Bold );

	// Initialize colors
	m_colorBackground = bgcolor;
	m_colorInfo = titlecolor;
	m_colorInactive = inactcolor;
	m_colorActive = actcolor;

	m_colors.clear();
	m_colors.push_back( m_colorBackground );
	m_streamColorIndex = -1;

	// Initialize the stream
	m_stream.clear();
	addEmpty();

	clearScreen();
}

void CDGGenerator::addSubcode( const SubCode& sc )
{
	m_stream.push_back( sc );
}

void CDGGenerator::addEmpty()
{
	SubCode sc;
	memset( &sc, 0, sizeof( sc ) );

	addSubcode( sc );
}

void CDGGenerator::fillColor( char * buffer, const QColor& color )
{
	// Green
	char red = (color.red() / 17) & 0x0F;
	char green = (color.green() / 17) & 0x0F;
	char blue = (color.blue() / 17) & 0x0F;

	// Red and green
	buffer[0] = (red << 2) | (green >> 2);
	buffer[1] = ((green & 0x03) << 5 ) | blue;
}

void CDGGenerator::clearScreen()
{
	SubCode sc;

	// First copy the colors if we got them
	if ( m_streamColorIndex != -1 && !m_colors.isEmpty() )
	{
		// Load first lower 8 colors
		SubCode sc;

		sc.command = CDG_COMMAND;
		sc.instruction = CDG_INST_LOAD_COL_TBL_0_7;
		memset( sc.data, 0, 16 );

		for ( int i = 0; i < 7; i++ )
		{
			if ( i >= m_colors.size() )
				break;

			fillColor( sc.data + i * 2, m_colors[i] );
		}

		m_stream[ m_streamColorIndex ] = sc;

		// Do we have more colors?
		if ( m_colors.size() > 8 )
		{
			sc.instruction = CDG_INST_LOAD_COL_TBL_8_15;
			memset( sc.data, 0, 16 );

			for ( int i = 8; i < 16; i++ )
			{
				if ( i >= m_colors.size() )
					break;

				fillColor( sc.data + i * 2, m_colors[i] );
			}

			m_stream[ m_streamColorIndex + 1 ] = sc;
		}

		m_colors.clear();
		m_colors.push_back( m_colorBackground );
	}

	// Now clear the screen
	for ( int i = 0; i < 16; i++ )
	{
		sc.command = CDG_COMMAND;
		sc.instruction = CDG_INST_MEMORY_PRESET;
		CDG_MemPreset* preset = (CDG_MemPreset*) sc.data;

		preset->repeat = i & 0x0F;
		preset->color = COLOR_IDX_BACKGROUND;

		addSubcode( sc );
	}

	sc.command = CDG_COMMAND;
	sc.instruction = CDG_INST_BORDER_PRESET;
	CDG_BorderPreset* preset = (CDG_BorderPreset*) sc.data;

	preset->color = COLOR_IDX_BACKGROUND;

	addSubcode( sc );

	// Reserve space for two LoadColor commands
	m_streamColorIndex = m_stream.size();
	addEmpty();
	addEmpty();
}

/*
void CDGGenerator::fillColor( char * buffer, const QColor& color )
{
	// Green
	char red = (color.red() / 17) & 0x0F;
	char green = (color.green() / 17) & 0x0F;
	char blue = (color.blue() / 17) & 0x0F;

	// Red and green
	buffer[0] = (red << 2) | (green >> 2);
	buffer[1] = ((green & 0x03) << 5 ) | blue;
}

void CDGGenerator::addLoadColors( const QColor& bgcolor, const QColor& titlecolor,
								  const QColor& actcolor, const QColor& inactcolor )
{
	SubCode sc;

	sc.command = CDG_COMMAND;
	sc.instruction = CDG_INST_LOAD_COL_TBL_0_7;

	// Fill the color data
	memset( sc.data, 0, 16 );
	fillColor( sc.data, bgcolor );
	fillColor( sc.data + 2, titlecolor );
	fillColor( sc.data + 4, inactcolor );
	fillColor( sc.data + 6, actcolor );

	addSubcode( sc );
}
*/
QString	CDGGenerator::stripHTML( const QString& str )
{
	QRegExp rx( "<.*>" );
	rx.setMinimal( true );;

	QString stripped = str;
	stripped.remove( rx );
	return stripped;
}

QByteArray CDGGenerator::stream()
{
	return QByteArray( (const char*) m_stream.data(), m_stream.size() * sizeof( SubCode ) );
}

int CDGGenerator::getColor( QRgb color )
{
	// Mask alpha channel
	color |= 0xFF000000;

	// Find in the array of colors
	for ( int i = 0; i < m_colors.size(); i++ )
		if ( m_colors[i] == QColor(color) )
			return i;

	if ( m_colors.size() == 16 )
		qFatal("Color table is out of color entries");

	qDebug("Adding color %08X", color );
	m_colors.push_back( color );
	return m_colors.size() - 1;
}

void CDGGenerator::checkTile( int offset_x, int offset_y, const QImage& orig,const QImage& newimg )
{
	// The first loop checks if there are any colors to change, and enumerates them
	QMap< int, QList<int> > color_changes;

	// Tiles are 6x12
	for ( int y = 0; y < 12; y++ )
	{
		const QRgb * orig_line = (const QRgb *) orig.scanLine( y + offset_y );
		const QRgb * new_line = (const QRgb *) newimg.scanLine( y + offset_y );

		for ( int x = 0; x < 6; x++ )
		{
//			int origcolor = getColor( orig.pixel( offset_x + x, offset_y + y ) );
//			int newcolor = getColor( newimg.pixel( offset_x + x, offset_y + y ) );

			int origcolor = getColor( orig_line[ offset_x + x ] );
			int newcolor = getColor( new_line[ offset_x + x ] );

			if ( origcolor == newcolor )
				continue;

			// Calculate the mask for the color change
			int mask = origcolor ^ newcolor;

			if ( (mask & 0xFFFFFFF0) != 0 )
				qFatal("error in mask calculation");

			// Store the coordinates in lo/hi bytes
			int coord = x << 8 | y;
			color_changes[ mask ].push_back( coord );
		}
	}

	// Anything to change?
	if ( color_changes.isEmpty() )
		return;

	// Enumerate the map entries
	const QList<int>& colors = color_changes.keys();

	// Bitmasks
	quint8 bitmask[6] = { 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	for ( int i = 0; i < colors.size(); i++ )
	{
		SubCode sc;

		sc.command = CDG_COMMAND;
		sc.instruction = CDG_INST_TILE_BLOCK_XOR;
		memset( sc.data, 0, 16 );
		CDG_Tile* tile = (CDG_Tile*) sc.data;

		tile->column = offset_x / 6;
		tile->row = offset_y / 12;
		tile->color0 = 0;
		tile->color1 = colors[ i ];

		const QList<int>& coords = color_changes[ colors[ i ] ];

		for ( int p = 0; p < coords.size(); p++ )
		{
			int x = coords[p] >> 8;
			int y = coords[p] & 0x0F;

			tile->tilePixels[y] |= bitmask[x];
		}

		addSubcode( sc );
	}
}

void CDGGenerator::applyTileChanges( const QImage& orig,const QImage& newimg )
{
/*
	static unsigned int i = 0;
	QString fname = QString("image-%1.bmp") .arg(i);

	qDebug("generating image %d", i );

	if ( ++i > 9 )
		i = 0;

	orig.save( "orig-" + fname, "bmp" );
	newimg.save( "new-" + fname, "bmp" );
*/
	// Tiles are 6x12
	for ( unsigned int offset_y = 0; offset_y < CDG_FULL_HEIGHT; offset_y += 12 )
		for ( unsigned int offset_x = 0; offset_x < CDG_FULL_WIDTH; offset_x += 6 )
			checkTile( offset_x, offset_y, orig, newimg );
}

void CDGGenerator::generate( const Lyrics& lyrics, qint64 total_length, const QString& title )
{
	LyricsRenderer renderer;
	QString	lastLyrics;

	// Prepare images
	QImage image( CDG_FULL_WIDTH, CDG_FULL_HEIGHT, QImage::Format_RGB32 );
	QImage lastImage( CDG_FULL_WIDTH, CDG_FULL_HEIGHT, QImage::Format_RGB32 );

	image.fill( m_colorBackground.rgb() );
	lastImage.fill( m_colorBackground.rgb() );

	// We're using QLabel for rendering as it is easier
	QLabel label;
	label.setAlignment( Qt::AlignCenter );
	label.setStyleSheet("background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 black, stop: 1 black);");

	label.setFont( m_renderFont );
	label.resize( CDG_FULL_WIDTH, CDG_FULL_HEIGHT );

	// Init lyrics renderer
	renderer.setPrefetch( 1000 );
	renderer.setLyrics( lyrics );
	renderer.setColors( m_colorActive.name(), m_colorInactive.name() );

	if ( !title.isEmpty() )
		renderer.setTitlePage( title );

	// Pop up progress dialog
	QProgressDialog dlg ("Rendering CD+G lyrics",
						 QString::null,
						 0,
						 99 );

	dlg.setMinimumDuration( 2000 );
	dlg.setValue( 0 );

	qint64 dialog_step = total_length / 100;

	// Render
	while ( 1 )
	{
		// There should be 300 packets in 1000ms of music
		// So each packet adds 1000 / 300 ms
		// Speed up time a little to compensate CD+G reader delay
		qint64 timing = m_stream.size() * 1000 / 300 + 250;

		// Should we show the next step?
		if ( timing / dialog_step > dlg.value() )
		{
			dlg.setValue( timing / dialog_step );
			qApp->processEvents( QEventLoop::ExcludeUserInputEvents );
		}

		if ( timing > total_length )
			return;

//		qDebug("timing: %d packets, %dms (%d sec)", m_stream.size(), (int) timing, (int) (timing / 1000) );

		QString lyrics = renderer.update( timing );

		// Did lyrics change at all?
		if ( lyrics == lastLyrics )
		{
			addEmpty();
			continue;
		}

		// Set up painter with disabled anti-aliasing to handle color detection
		image.fill( m_colorBackground.rgb() );
		QPainter painter( &image );
		painter.setRenderHints( QPainter::Antialiasing
							 | QPainter::TextAntialiasing
							 | QPainter::SmoothPixmapTransform
							 | QPainter::HighQualityAntialiasing
							 | QPainter::NonCosmeticDefaultPen, false );

		// Render into text label and grab the content
		label.setText( lyrics );
		label.update();
		label.render( &painter );

		// Is change significant enough to warrant full redraw?
		if ( stripHTML( lyrics ) != stripHTML( lastLyrics ) )
		{
			clearScreen();

			// Clear the old image too
			lastImage.fill( m_colorBackground.rgb() );
		}

		applyTileChanges( lastImage, image );
		lastImage = image;
		lastLyrics = lyrics;
	}
}
