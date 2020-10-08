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

const char * identifier;

cnovr_terminal * terminal;
cnovr_model * overlaymodel;
cnovrfocus_capture overlaycapture;
cnovr_point3d overlaysize;
VROverlayHandle_t ulOverlayHandle;

int quit;
int keyboard_listen_socket_tcp;
og_thread_t keyboard_input_thread;

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

void init( const char * identifier )
{
	ovrprintf( "Example init %s\n", identifier );
}

void UpdateFunction( void * tag, void * opaquev )
{
	CNOVRTerminalUpdate( terminal );
	CNOVRTerminalFlipTex( terminal );

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


void RenderFunction( void * tag, void * opaquev )
{
}

int OverlayFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	int r = CNOVRFocusDefaultFocusEvent( event, cap, prop, buttoninfo );
	if( event == CNOVRF_LOSTFOCUS )
	{
		printf( "SAVE:%s\n", identifier );
		CNOVRNamedPtrSave( identifier );
	}
	return r;
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
		CNOVRTerminalFeedback( terminal, buf, r );
	}
	//Close will happen on cleanup.
	return 0;
}

void * KeyboardListenSocketThread( void * v )
{
	struct sockaddr_in clientaddr; /* client addr */
	int   clientlen = sizeof(clientaddr);
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

	overlaysize[0] = 2.5;
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

	cnovrstate->oOverlay->SetOverlayWidthInMeters( ulOverlayHandle, 2.5 );
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

	printf( "+++ overlay scene setup complete\n" );
}


void start( const char * lidentifier )
{
	identifier = strdup(lidentifier);
	CNOVRJobTack( cnovrQPrerender, overlay_scene_setup, 0, 0, 0 );
	printf( "=== overlay start %s(%p) + %p %p\n", identifier, identifier );
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


