#include "cnovrterminal.h"
#include "cnovr.h"
#include "vlinterm.h"
#include "cnovrtccinterface.h"
#include "cnovrutil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CHAR_DOUBLE 1


#if !defined(WINDOWS) && !defined(WIN32) && !defined(WIN64)
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define POSIX_STUFF
#endif


static void CNOVRTerminalDelete( cnovr_terminal * ths )
{
	if( !ths ) return;
	if( ths->apprxthread )
	{
		ths->quit = 1;
#ifdef POSIX_STUFF
		kill( ths->ts.pid, SIGINT );
#endif
		close( ths->ts.ptspipe );
		OGJoinThread( ths->apprxthread );
	}

	CNOVRDelete( ths->canvas );
	if( ths->title ) free( ths->title );
	CNOVRFreeLater( ths );
}

static void CNOVRTerminalRender( cnovr_terminal * ths )
{
	if( ths->canvas )
		CNOVRRender( ths->canvas );
}

cnovr_header cnovr_terminal_header = {
	(cnovrfn)CNOVRTerminalDelete, //Delete
	(cnovrfn)CNOVRTerminalRender, //Render
	TYPE_TERMINAL,
};

static void * TermRXThread( void * v )
{
	cnovr_terminal * term = v;

    while( !term->quit )
	{
		uint8_t rxdata[1024+1];
		int rx = read( term->ts.ptspipe, rxdata, 1024 );
		if( rx < 0 ) break;
		int i;
		for( i = 0; i < rx; i++ )
		{
			char crx = rxdata[i];
			EmitChar( &term->ts, crx );
		}
	}
	fprintf( stderr, "Error: Terminal pipe died\n" ); 
	close( term->ts.ptspipe );
	return 0;
}

static uint32_t TermColor( uint32_t incolor, int color, int attrib )
{
	//XXX TODO: Do something clever here to allow for cool glyphs.
	int actc;
	int in_inten = (incolor&0xff) + ( (incolor&0xff00)>>8 ) + (( incolor&0xff0000 )>> 16 );

	if( (in_inten > 384 ) ^ (!!(attrib&(1<<4))) )
	{
		actc = color & 0x0f;
	}
	else
	{
		actc = (color >> 4) & 0x0f;
	}

	int base_color = (attrib & 1)?0xff:0xaf;
	if( attrib & 1 ) base_color = 0xff; //??? This looks wrong?

	uint32_t outcolor = 0;// = incolor
	if( actc & 4 ) outcolor |= base_color;
	if( actc & 2 ) outcolor |= base_color<<8;
	if( actc & 1 ) outcolor |= base_color<<16;

	//On this system, we flip RED and BLUE.
	uint8_t red = outcolor & 0xff;
	uint8_t blue = (outcolor >> 16) & 0xff;
	outcolor = ( outcolor & 0x00ff00 ) | blue | (red << 16 );
	return outcolor | 0xff000000;
}


static int charset_w, charset_h, font_w, font_h;
static uint32_t * font;

static int LoadFont(cnovr_terminal * term, const char * fontfile)
{
	if( font ) return 0;

	FILE * f = fopen( fontfile, "rb" );
	int c = 0, i;
	int format;
	if( !f ) { fprintf( stderr, "Error: cannot open font file %s\n", fontfile ); return -1; }
	if( (c = fgetc(f)) != 'P' ) { fprintf( stderr, "Error: Cannot parse first line of font file [%d].\n", c ); return -2; } 
	format = fgetc(f);
	fgetc(f);
	while( (c = getc(f)) != EOF ) { if( c == '\n' ) break; }	//Comment
	if( fscanf( f, "%d %d\n", &charset_w, &charset_h ) != 2 ) { fprintf( stderr, "Error: No size in pgm.\n" ); return -3; }
	while( (c = getc(f)) != EOF ) if( c == '\n' ) break;	//255
	if( (charset_w & 0x0f) || (charset_h & 0x0f) ) { fprintf( stderr, "Error: charset must be divisible by 16 in both dimensions.\n" ); return -4; }
	font = malloc( charset_w * charset_h * 4 );
	for( i = 0; i < charset_w * charset_h; i++ ) { 
		if( format == '5' )
		{
			c = getc(f);
			font[i] = c | (c<<8) | (c<<16);
		}
		else
		{
			fprintf( stderr, "Error: unsupported font image format.  Must be P5\n" );
			return -5;
		}
	}
	font_w = charset_w / 16;
	font_h = charset_h / 16;
	fclose( f );
	return 0;
}


cnovr_terminal * CNOVRTerminalCreate( const char * name, int cols, int rows, int properties )
{
	cnovr_terminal * ret = malloc( sizeof( cnovr_terminal ) );
	memset( ret, 0, sizeof( cnovr_terminal ) );

	ret->base.header = &cnovr_terminal_header;
	ret->base.tccctx = TCCGetTag();
	ret->ts.screen_mutex = OGCreateMutex();
	ret->ts.charx = cols;
	ret->ts.chary = rows;
	ret->ts.echo = 0;
	ret->ts.historyy = 1000;
	ret->ts.termbuffer = 0;
	ret->title = strdup( name );
	ret->ts.user = ret;
	ret->taint_all = true;


	if( LoadFont( ret, "cntools/vlinterm/ibm437.pgm") )
	{
		ovrprintf( "Error loading font\n" );
		goto fail;
	}

	short w = ret->ts.charx * font_w * (CHAR_DOUBLE+1);
	short h = ret->ts.chary * font_h * (CHAR_DOUBLE+1);

	ret->canvas = CNOVRCanvasCreate( name, w, h, properties );

	ResetTerminal( &ret->ts );
	ResizeScreen( &ret->ts, ret->ts.charx, ret->ts.chary );

	return ret;
fail:
	if( ret )
	{
		CNOVRDelete( ret );
	}
	return 0;
	
}

void CNOVRTerminalRunCommand( cnovr_terminal * term, int argc, char ** argv )
{
	if( term->apprxthread )
	{
		term->quit = 1;
#ifdef POSIX_STUFF
		kill( term->ts.pid, SIGINT );
#endif
		close( term->ts.ptspipe );
		OGJoinThread( term->apprxthread );
	}

	term->quit = 0;
	ResetTerminal( &term->ts );
#ifdef POSIX_STUFF
	term->ts.ptspipe = spawn_process_with_pts( "/bin/bash", argv, &term->ts.pid );
#endif
	ResizeScreen( &term->ts, term->ts.charx, term->ts.chary );
	term->apprxthread = OGCreateThread( TermRXThread, (void*)term );
}


void CNOVRTerminalUpdate( cnovr_terminal * term )
{
	cnovr_canvas * c = term->canvas;
	uint32_t * framebuffer = c->data;

	//Not storing ts as poiner reference for &term->ts, because doing this inline
	//is just a single indirect read.  The same performance.  This is actually better
	//Because we only need one `this`.
	#define TS (term->ts)

	short screenx = c->w;
	short screeny = c->h;
	int x, y;
	int w = c->w;
	int h = c->h;

	term->iUpdateNumber++;

/*
	XXX TODO - Allow change of terminal size.
	int newx = screenx/font_w/(CHAR_DOUBLE+1);
	int newy = screeny/font_h/(CHAR_DOUBLE+1);

	if( ((newx != term->TS.charx || newy != term->TS.chary) && newy > 0 && newx > 0) || !framebuffer )
	{
		ResizeScreen( &TS, newx, newy );
		w = term->TS.charx * font_w * (CHAR_DOUBLE+1);
		h = term->TS.chary * font_h * (CHAR_DOUBLE+1);
		framebuffer = realloc( framebuffer, w*h*4 );
		memset( framebuffer, 0, w*h*4 );
		if( last_cury >= TS.chary ) last_cury = TS.chary-1;
	}
*/

	int scrollback = TS.scrollback;
	if( scrollback >= TS.historyy-TS.chary ) scrollback = TS.historyy-TS.chary-1;
	if( scrollback < 0 ) scrollback = 0;
	int taint_all = ( scrollback != term->last_scrollback ) || term->taint_all;

	if( term->last_curx != TS.curx || term->last_cury != TS.cury || TS.tainted || taint_all )
	{
		OGLockMutex( TS.screen_mutex );
		uint32_t * tb = TS.termbuffer;
		TS.tainted = 0;
		term->taint_all = 0;
		if( term->last_curx < TS.charx && term->last_cury < TS.chary )
			tb[term->last_curx+term->last_cury*TS.charx] |= 1<<24;
		if( TS.curx < TS.charx && TS.cury < TS.chary )
			tb[TS.curx + TS.cury * TS.charx] |= 1<<24;
		term->last_curx = TS.curx;
		term->last_cury = TS.cury;

		for( y = 0; y < TS.chary; y++ )
		for( x = 0; x < TS.charx; x++ )
		{
			int ly = y - scrollback;

			uint32_t v = tb[x+ly*TS.charx];
			if( !taint_all && !(v & (1<<24)) ) continue;
			tb[x+ly*TS.charx] &= ~(1<<24);
			unsigned char c = v & 0xff;
			int color = v>>16;
			int attrib = v>>8;

			int cx, cy;
			int atlasx = c & 0x0f;
			int atlasy = c >> 4;
			uint32_t * fbo =   &framebuffer[x*font_w*(CHAR_DOUBLE+1) + y*font_h*w*(CHAR_DOUBLE+1)];
			uint32_t * start = &font[atlasx*font_w+atlasy*font_h*charset_w];
			int is_cursor = (x == TS.curx && ly == TS.cury) && (TS.dec_private_mode & (1<<25));
			if( ( term->iUpdateNumber & 1 ) && is_cursor ) is_cursor = 0;
	#if CHAR_DOUBLE==1
			for( cy = 0; cy < font_h; cy++ )
			{
				for( cx = 0; cx < font_w; cx++ )
				{
					fbo[cx*2+0] = fbo[cx*2+1] = (is_cursor?0xefffffff:0)^TermColor( start[cx], color, attrib );
				}
				fbo += w;
				for( cx = 0; cx < font_w; cx++ )
				{
					fbo[cx*2+0] = fbo[cx*2+1] = (is_cursor?0xefffffff:0)^TermColor( start[cx], color, attrib );
				}
				fbo += w;
				start += charset_w;
			}
	#else
			for( cy = 0; cy < font_h; cy++ )
			{
				for( cx = 0; cx < font_w; cx++ )
				{
					fbo[cx] = (is_cursor?0xefffffff:0)^TermColor( start[cx], color, attrib );
				}
				fbo += w;
				start += charset_w;
			}
	#endif
		}
		OGUnlockMutex( TS.screen_mutex );
		//CNFGUpdateScreenWithBitmap( (unsigned long *)framebuffer, w, h );

		term->tainted = 1;
	}

	term->last_scrollback = scrollback;

}

void CNOVRTerminalFlipTex( cnovr_terminal * term )
{
	if( term->tainted )
	{
		CNOVRCanvasSwapBuffers( term->canvas );
		term->tainted = 0;
	}
}

void CNOVRTerminalEmitStr( cnovr_terminal * term, const uint8_t * strs, int len )
{
	int i;
	if( len < 0 ) len = strlen( (const char *)strs );
	for( i = 0; i < len; i++ )
	{
		EmitChar( &term->ts, strs[i] );
	}
}

void CNOVRTerminalFeedback( cnovr_terminal * term, const uint8_t * strs, int len )
{
	if( len < 0 ) len = strlen( (const char *)strs );
#ifdef POSIX_STUFF
	FeedbackTerminal( &term->ts, strs, len );
#endif
}

void HandleBell( struct TermStructure * ts )
{
}

void HandleOSCCommand( struct TermStructure * ts, int parameter, const char * value )
{
	cnovr_terminal * term = (cnovr_terminal *)ts->user;
	if( parameter == 0 )
	{
		char * todel = term->title;
		term->title = strdup( value );
		if( todel ) CNOVRFreeLater( todel );
	}
	else
		ovrprintf( "OSC Command: %d: %s\n", parameter, value );
}



