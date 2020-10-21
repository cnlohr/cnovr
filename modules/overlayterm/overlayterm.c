#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrterminal.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <openvr_capi.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>

const char * identifier;

cnovr_terminal * terminal;
cnovr_model * overlaymodel;
cnovrfocus_capture overlaycapture;
cnovr_point3d overlaysize;
VROverlayHandle_t ulOverlayHandle;

int quit;
int keyboard_listen_socket_tcp;
og_thread_t keyboard_input_thread;

int menu_up;
int minified;

struct focusinfo
{
	int terminal;
	float xpx;
	float ypx;
	double time;
} focusdev[3];

struct staticstore
{
	int initialized;
	cnovr_pose pOverlayLocation;
} * store;

struct listener_socket
{
	og_thread_t listenerthread;
	struct listener_socket * next;
	int socket;
};
struct listener_socket * kblhead;


void FunctionTop( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	CNOVRTerminalFeedback( terminal, (uint8_t*)"top\n", 4 );
}

void FunctionCloseSystem( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	printf( "Exiting steamvr\n" );
	//system( "ps -ef | grep 'vrserver' | grep -v grep | awk '{print $2}' | xargs -r kill -9" );
}

void FunctionCtrlC( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	CNOVRTerminalFeedback( terminal, (uint8_t*)"\x03", 1 ); //VT-100 ctrl+c
}

void FunctionCtrlZ( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	CNOVRTerminalFeedback( terminal, (uint8_t*)"\x1a", 1 ); //VT-100 ctrl+z
}

void FunctionQ( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	CNOVRTerminalFeedback( terminal, (uint8_t*)"q", 1 ); //VT-100 ctrl+z
}

void FunctionUptime( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	CNOVRTerminalFeedback( terminal, (uint8_t*)"uptime\n", 7 );
}

#define NUM_HISTORY 8
#define MAX_HIST_LEN 1024
char history[NUM_HISTORY][MAX_HIST_LEN];

void FunctionHistory( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	char cmd[MAX_HIST_LEN+4];
	int len = snprintf( cmd, MAX_HIST_LEN+3, "%s\n", element->vopaque );
	CNOVRTerminalFeedback( terminal, cmd, len );
}

void FunctionMinifyTerminal( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid )
{
	menu_up = 0;
	minified = 1;
	store->pOverlayLocation.Scale *= 0.1;
}


//Canned GUI
cnovr_canvas_canned_gui_element termgui[] = {
	{ 900, 45, 200, 50, FunctionMinifyTerminal, "Minify Terminal", 0, 0, 0, 0, 7, 0x10101010 },

	{ 190, 90, 200, 100, FunctionUptime, "Uptime", 0, 0, 0, 0, 7, 0x10101010 },
	{ 190, 90, 200, 200, FunctionTop, "Top", 0, 0, 0, 0, 7, 0x10101010 },
	{ 190, 45, 200, 300, FunctionCtrlC, "Ctrl+C", 0, 0, 0, 0, 7, 0x10101010 },
	{ 190, 45, 200, 350, FunctionCtrlZ, "Ctrl+Z", 0, 0, 0, 0, 7, 0x10101010 },
	{ 190, 45, 200, 400, FunctionQ, "\'Q\'", 0, 0, 0, 0, 7, 0x10101010 },

	{ 690, 45, 400, 100, FunctionHistory, history[0], history[0], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 150, FunctionHistory, history[1], history[1], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 200, FunctionHistory, history[2], history[2], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 250, FunctionHistory, history[3], history[3], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 300, FunctionHistory, history[4], history[4], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 350, FunctionHistory, history[5], history[5], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 400, FunctionHistory, history[6], history[6], 0, 0, 0, 7, 0x10101010 },
	{ 690, 45, 400, 450, FunctionHistory, history[7], history[7], 0, 0, 0, 7, 0x10101010 },

	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0 },
};

void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	cnovrstate->bCanHMDFocus = 1;

	if( !minified )
	{
		CNOVRTerminalUpdate( terminal );
	}

	int cursors = 0;
	cnovr_canvas * canvas = terminal->canvas;
	int i;
	for( i = 0; i < sizeof(focusdev)/sizeof(focusdev[0]); i++ )
	{
		struct focusinfo * fi = focusdev + i;
		double elapse = OGGetAbsoluteTime() - fi->time;
		if( elapse > .2 ) continue;
		CNOVRCanvasColor( canvas, CNOVRHSVtoHEX( 0, 0, 1.1-elapse*5. ) | 0xf000000 );
		CNOVRCanvasTackRectangle( canvas, (int)fi->xpx-2, (int)fi->ypx-2, (int)fi->xpx+2, (int)fi->ypx+2 );
		cursors++;

		if( !minified &&  !menu_up )
		{
			if( fi->xpx < 100 && fi->ypx < 100 )
			{
				menu_up = 1;
			}
		}
	}


	if( !minified )
	{
		if( cursors )
		{
			terminal->taint_all = true;
			CNOVRCanvasSwapBuffers( canvas );
		}
		else
		{
			menu_up = false;
			CNOVRTerminalFlipTex( terminal ); //Does not swap if untainted - not what we want.
		}


		if( menu_up )
		{
			char bash_history_path[1024];
		    char *homedir = getenv("HOME");
			snprintf( bash_history_path, sizeof( bash_history_path ), "%s/.bash_history", homedir ); 
			FILE * flh = fopen( bash_history_path, "rb" );
			if( flh )
			{
				char * bash_history;
				fseek( flh, 0, SEEK_END );
				int len = ftell( flh );
				fseek( flh, 0, SEEK_SET );
				bash_history = malloc( len + 1 );
				fread( bash_history, 1, len, flh );
		 		bash_history[len] = 0;
				fclose( flh );

				int bash_count;
				char ** bash_history_lines = CNOVRSplitStrings( bash_history, "\n", "", 1, &bash_count );
				int i;
				int lpl = bash_count-1;
				memset( history, 0, sizeof(history) );
				for( i = NUM_HISTORY-1; i >= 0 && lpl >= 0; i-- )
				{
					do
					{
						int j;
						if( lpl < 0 ) break;
						char * hist = bash_history_lines[lpl];

						lpl--;

						for( j = i; j < NUM_HISTORY; j++ )
							if( strcmp( history[j], hist ) == 0 ) break;

						if( j == NUM_HISTORY )
						{
							strncpy( history[i], hist, MAX_HIST_LEN - 1 );
							history[i][MAX_HIST_LEN-1] = 0;
							break;
						}
					} while( 1 );
				}

				free(bash_history_lines );
				free(bash_history);
			}

			CNOVRCanvasDialogColor( canvas,  CNOVRHSVtoHEX( 0, .1, .1 ) | 0xff000000 );
			CNOVRCanvasBGColor( canvas, 0x00000040 );
			CNOVRCanvasSetOrMask( canvas, 0xff3f3f3f );
			CNOVRCanvasApplyCannedGUI( canvas, termgui );
		}
		else
		{
			CNOVRCanvasSetOrMask( canvas, 0xff3f3f3f );
			CNOVRCanvasColor( canvas, CNOVRHSVtoHEX( 0, .1, .1 ) | 0xff000000 );
			CNOVRCanvasTackRectangle( canvas, 0, 0, 100, 100 );
		}

		struct VRTextureBounds_t vbOverlayTextureBounds;
		vbOverlayTextureBounds.uMin = 0;
		vbOverlayTextureBounds.uMax = 1;
		vbOverlayTextureBounds.vMin = 1;
		vbOverlayTextureBounds.vMax = 0;
		cnovrstate->oOverlay->SetOverlayTextureBounds( ulOverlayHandle, &vbOverlayTextureBounds );

		Texture_t t;
		t.handle = (void*)(uintptr_t)terminal->canvas->model->pTextures[0]->nTextureId;
		t.eType = ETextureType_TextureType_OpenGL;
		t.eColorSpace = EColorSpace_ColorSpace_Auto;

		cnovrstate->oOverlay->SetOverlayTexture( ulOverlayHandle, &t );
		FLT m44[16];	pose_to_matrix44( m44, &store->pOverlayLocation );
		cnovrstate->oOverlay->SetOverlayTransformAbsolute( ulOverlayHandle, ETrackingUniverseOrigin_TrackingUniverseStanding, (struct HmdMatrix34_t *)m44);
	}
	else if( minified == 1 )
	{
		CNOVRCanvasClearFrame( canvas );
		CNOVRCanvasSwapBuffers( canvas );

		Texture_t t;
		t.handle = (void*)(uintptr_t)terminal->canvas->model->pTextures[0]->nTextureId;
		t.eType = ETextureType_TextureType_OpenGL;
		t.eColorSpace = EColorSpace_ColorSpace_Auto;

		cnovrstate->oOverlay->SetOverlayTexture( ulOverlayHandle, &t );
		FLT m44[16];	pose_to_matrix44( m44, &store->pOverlayLocation );
		cnovrstate->oOverlay->SetOverlayTransformAbsolute( ulOverlayHandle, ETrackingUniverseOrigin_TrackingUniverseStanding, (struct HmdMatrix34_t *)m44);

		minified = 2;
	}
	else if( minified == 2 )
	{
	}
}


void RenderFunction( void * tag, void * opaquev )
{
}

int OverlayFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	int do_regular = 1;

	if( minified )
	{
		if( event == CNOVRF_DOWNNOFOCUS || event == CNOVRF_DOWNFOCUS )
		{
			minified = 0;
			store->pOverlayLocation.Scale *= 10;
		}
		return -1;
	}

	if( event == CNOVRF_LOSTFOCUS )
	{
		CNOVRNamedPtrSave( identifier );
	}
	if( event == CNOVRF_MOTION )
	{
		int did = prop->devid;
		if( did < sizeof( focusdev ) / sizeof(focusdev[0]) )
		{
			struct focusinfo * fi = focusdev + did;
			fi->terminal = &overlaycapture == prop->capturedPassive;
			fi->xpx = (prop->NewPassiveProps[0]) * terminal->canvas->w;
			prop->NewPassiveProps[1] = 1. - prop->NewPassiveProps[1];
			fi->ypx = (prop->NewPassiveProps[1]) * terminal->canvas->h;
			fi->time = OGGetAbsoluteTime();
			//printf( "%d %d %f %f  %f\n", did, fi->terminal, fi->xpx, fi->ypx, fi->time );
		}
	}
	if( menu_up )
	{
		//Dirty, if we're doing the menu, mess with the cap.
		cap->opaque = terminal->canvas;
		do_regular = !CNOVRCanvasFocusEvent( event, cap, prop, buttoninfo );
		cap->opaque = overlaymodel;
	}
	if( do_regular )
	{
		cnovr_model * m = cap->opaque;
		CNOVRGeneralHandleFocusEvent(  m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_TRIGGER );
	}
	return 1;
}


void * KeyboardDataSocketThread( void * v )
{
	struct listener_socket * ls = (struct listener_socket*)v;
	int r;
	char buf[16384];
	while( !quit )
	{
		r = recv( ls->socket, buf, sizeof(buf), 0);
		if( r <= 0 )
		{
			ovrprintf( "Read failed on socket.  Code: %d\n", r );
			break;
		}
		CNOVRTerminalFeedback( terminal, (uint8_t*)buf, r );
	}
	//Close will happen on cleanup.
	return 0;
}

void * KeyboardListenSocketThread( void * v )
{
	struct sockaddr_in clientaddr; /* client addr */
	socklen_t  clientlen = sizeof(clientaddr);
	while( !quit )
	{
		int  childfd = accept(keyboard_listen_socket_tcp, (struct sockaddr *) &clientaddr, &clientlen);
		if (childfd < 0) 
			ovrprintf( "Failed to accept to keyboard socket.\n" );
		struct listener_socket * ns = malloc( sizeof( struct listener_socket ) );
		ns->socket = childfd;
		ns->listenerthread = OGCreateThread( KeyboardDataSocketThread, (void*)ns );
		ns->next = kblhead;
		kblhead = ns;
	}
	return 0;
}


static void overlay_scene_setup( void * tag, void * opaquev )
{
	terminal = CNOVRTerminalCreate( "testterm", 80, 25 );

	store = CNOVRNamedPtrData( identifier, 0, sizeof( *store ) + 1024 );
	if( !store->initialized )
	{
		pose_make_identity( &store->pOverlayLocation );
		store->pOverlayLocation.Pos[0] = 1;
		store->pOverlayLocation.Pos[1] = 1.;
		store->pOverlayLocation.Pos[2] = -4.;
		store->initialized = 1;
	}

	overlaysize[0] = 1.75;
	overlaysize[1] = 1;
	overlaysize[2] = 0;

	overlaymodel = CNOVRModelCreate( 0, GL_TRIANGLES );
	overlaymodel->pose = &store->pOverlayLocation;
	CNOVRModelAppendMesh( overlaymodel, 1, 1, 0, overlaysize, 0, 0 );
	overlaycapture.tag = 0;
	overlaycapture.opaque = overlaymodel;
	overlaycapture.cb = OverlayFocusEvent;
	CNOVRModelSetInteractable( overlaymodel, &overlaycapture );

	if( !cnovrstate->oOverlay )
	{
		ovrprintf( "OpenVR Interface Could not get a handle on an overlay object.\n" );
		return;
	}

	ulOverlayHandle = 0;
	EVROverlayError e = cnovrstate->oOverlay->CreateOverlay( "cnovroverlay", "appname", &ulOverlayHandle );
	if( e )
	{
		ovrprintf( "OpenVR Create Overlay Failed. %d\n", e );
		return;
	}

	cnovrstate->oOverlay->SetOverlayWidthInMeters( ulOverlayHandle, overlaysize[0]*2.0 );
	cnovrstate->oOverlay->SetOverlayColor( ulOverlayHandle, 1., 1., 1. );
	cnovrstate->oOverlay->ShowOverlay( ulOverlayHandle );

	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	char * argv[2];
	argv[0] = strdup( "/bin/bash" );
	argv[1] = 0;
	CNOVRTerminalRunCommand( terminal, 1, argv );

	struct sockaddr_in serveraddr; /* server's addr */

	keyboard_listen_socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
	int  optval = 1;
	setsockopt(keyboard_listen_socket_tcp, SOL_SOCKET, SO_REUSEADDR, 
		(const void *)&optval , sizeof(int));
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(23889);
	if (bind(keyboard_listen_socket_tcp, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
		ovrprintf( "Failed to bind keyboard socket.\n" );
	if (listen(keyboard_listen_socket_tcp, 5) < 0) /* allow 5 requests to queue up */ 
		ovrprintf( "Failed to listen to keyboard socket.\n" );
	keyboard_input_thread = OGCreateThread( KeyboardListenSocketThread, 0 );

	//And on UDP
	{
		struct sockaddr_in uservaddr;	
		memset(&uservaddr, 0, sizeof(uservaddr)); 
		uservaddr.sin_family    = AF_INET; // IPv4 
		uservaddr.sin_addr.s_addr = INADDR_ANY; 
		uservaddr.sin_port = htons(23889); 
		int usock; 
		if ( (usock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
			ovrprintf("UDP socket creation failed"); 
		} 
		setsockopt(usock, SOL_SOCKET, SO_REUSEADDR, 
			(const void *)&optval , sizeof(int));

		// Bind the socket with the server address 
		if ( bind(usock, (const struct sockaddr *)&uservaddr,  sizeof(uservaddr)) < 0 ) 
		{ 
			ovrprintf("UDP bind failed"); 
			exit(EXIT_FAILURE); 
		} 
    
		struct listener_socket * ns = malloc( sizeof( struct listener_socket ) );
		ns->socket = usock;
		ns->listenerthread = OGCreateThread( KeyboardDataSocketThread, (void*)ns );
		ns->next = kblhead;
		kblhead = ns;
	}

	printf( "+++ overlay scene setup complete\n" );
}


void start( const char * lidentifier )
{
	identifier = strdup(lidentifier);
	CNOVRJobTack( cnovrQPrerender, overlay_scene_setup, 0, 0, 0 );
	printf( "=== overlay start %s(%p)\n", identifier, identifier );
}

void stop( const char * identifier )
{

	quit = 1;
	if( keyboard_listen_socket_tcp) close( keyboard_listen_socket_tcp );
	if( keyboard_input_thread ) OGJoinThread( keyboard_input_thread );
	if( ulOverlayHandle )
	{
		cnovrstate->oOverlay->DestroyOverlay( ulOverlayHandle );
	}
	while( kblhead )
	{
		if( kblhead->socket ) close( kblhead->socket );
		if( kblhead->listenerthread ) OGJoinThread( kblhead->listenerthread );
		kblhead = kblhead->next;
	}
	printf( "=== End overlay stop\n" );
}


