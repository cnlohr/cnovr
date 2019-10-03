#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <errno.h>

int handle;

cnovr_model * model;
cnovr_shader * shader;
cnovr_texture * texture;
cnovr_pose pose;


/////////////////////////////////////////////////////////////////////////////////
// Display stuff

og_thread_t gtt;

int need_texture = 1;
Window windowtrack;
static XImage *image;
XShmSegmentInfo shminfo;
int width, height;
Display * localdisplay;
int quitting;


int Xerrhandler(Display * d, XErrorEvent * e)
{
	printf( "XShmGetImage error\n" );
    return 0;
}


void ListWindows()
{
	double start = OGGetAbsoluteTime();

    bool found = false;
    Window rootWindow = RootWindow( localdisplay, DefaultScreen(localdisplay));
    Atom atom = XInternAtom(localdisplay, "_NET_CLIENT_LIST", true);
    Atom actualType;
    int format;
    unsigned long numItems;
    unsigned long bytesAfter;
    
    unsigned char *data = '\0';
    Window *list;    
    char *windowName;

    int status = XGetWindowProperty(localdisplay, rootWindow, atom, 0L, (~0L), false,
        AnyPropertyType, &actualType, &format, &numItems, &bytesAfter, &data);
    list = (Window *)data;
    
    if (status >= Success && numItems) {
        for (int i = 0; i < numItems; ++i) {
            status = XFetchName(localdisplay, list[i], &windowName);
            if (status >= Success) {
				if( strstr( windowName, "Firefox" ) != 0 )
				{
					windowtrack = list[i];
					found = 1;
					break;
				}
            }
        }
    }
    XFree(windowName);
    XFree(data);

	if( found )
	{
		XWindowAttributes attribs;
	    XGetWindowAttributes(localdisplay, windowtrack, &attribs);

	    int width = attribs.width;
	    int height = attribs.height;
		printf( "%d %d\n", width, height );
	}
	else
	{
		printf( "Window not found\n" );
	}

	printf( "Time spent: %f\n", OGGetAbsoluteTime() - start );
}


void * GetTextureThread( void * v )
{
	XInitThreads();
	localdisplay = XOpenDisplay(NULL);
	XSetErrorHandler(Xerrhandler);
	ListWindows();

	while( !quitting )
	{
		if( windowtrack && need_texture == 1 )
		{
			XWindowAttributes attribs;
			XGetWindowAttributes(localdisplay, windowtrack, &attribs);
			int taint = 0;
			if( attribs.width != width ) taint = 1;
			if( attribs.height != height ) taint = 1;
			width = attribs.width;
			height = attribs.height;
		//	printf( "%d %d\n", width, height );

			if( shminfo.shmid <= 0 )
			{

				printf( "getting SHM\n" );
				shminfo.shmid = shmget(IPC_PRIVATE,
					2048*4 * 2048,
					IPC_PRIVATE | IPC_EXCL | IPC_CREAT|0777);
				printf( "%d ERRNO %d %d %d    %p\n", shminfo.shmid, errno, image );
			}

			if( taint  || !image)
			{
				image = XShmCreateImage( localdisplay, 
					attribs.visual, attribs.depth,
					ZPixmap, NULL, &shminfo, width, height); 

				shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
				shminfo.readOnly = False;
				printf( "Attaching\n" );
				XShmAttach(localdisplay, &shminfo);
			}
	 
			if( shminfo.shmid > 0 )
			{
				double ds = OGGetAbsoluteTime();
				XShmGetImage(localdisplay, windowtrack, image, 0, 0, AllPlanes);
				double part1 = OGGetAbsoluteTime() - ds;
				if( image && image->data && (void*)(&(image->data[0])) != (void*)(-1) )
				{
					need_texture = 0;
				}
				if( part1 > .0025 )
					printf( "GET %10f\n", part1 );
			}
		}
		OGUSleep( 1000 );
	}
	return 0;
}



void PreRender()
{
	if( !need_texture )
	{
		double ds = OGGetAbsoluteTime();
		CNOVRTextureLoadDataNow( texture, image->width, image->height, 4, 0,  (void*)(&(image->data[0])), 1 );
		double part1 = OGGetAbsoluteTime() - ds;
		if( part1 > 0.005 ) printf( "Loading Data %f\n", part1 );
		need_texture = 1;
	}
}

void Render()
{
	CNOVRRender( shader );
	CNOVRRender( texture );
	CNOVRModelRenderWithPose( model, &pose );
}

void GenGeo()
{
	printf( "Genning\n" );
	pose_make_identity( &pose );
	pose.Rot[0] = .707; pose.Rot[1] = .707;
	shader = CNOVRShaderCreate( "rendermodel" );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	printf( "CREATING MESH\n" );
	CNOVRModelAppendMesh( model, 2, 2, 1, 1 );
	printf( "DONE MESH\n" );
//	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0 );
//	CNOVRModelApplyTextureFromFileAsync( model, "test.png" );
	texture = CNOVRTextureCreate( 0, 0, 0 ); 
}


void prerender_startup( void * tag, void * opaquev )
{
	GenGeo();
	ListWindows();
	CNOVRListAdd( cnovrLRender, &handle, Render );
	CNOVRListAdd( cnovrLPrerender, &handle, PreRender );
}

void start( const char * identifier )
{
	printf( "Dockables start %s(%p)\n", identifier, identifier );
	CNOVRJobTack( cnovrQPrerender, prerender_startup, 0, 0, 0 );
	gtt = OGCreateThread( GetTextureThread, 0 );
	printf( "Dockables start OK %s\n", identifier );

}

void stop( const char * identifier )
{
	printf( "Stopping\n" );
	quitting = 1;
	XCloseDisplay( localdisplay );
	printf( "Joining\n" );
	OGJoinThread( gtt );
	printf( "Stopped\n" );

	CNOVRListDeleteTag( &handle );
	printf( "Dockables Destroying: %p %p\n",shader, model );
	CNOVRDelete( shader );
	CNOVRDelete( model );
	printf( "Dockables End stop\n" );
}

