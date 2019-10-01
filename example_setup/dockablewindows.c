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

extern XWindowAttributes CNFGWinAtt;
extern XClassHint *CNFGClassHint;
extern Display *CNFGDisplay;
extern Window CNFGWindow;
extern Pixmap CNFGPixmap;
extern GC     CNFGGC;
extern GC     CNFGWindowGC;
extern Visual * CNFGVisual;

int handle;

cnovr_model * model;
cnovr_shader * shader;
cnovr_texture * texture;
cnovr_pose pose;

Window windowtrack;

static XImage *image;
XShmSegmentInfo shminfo;
int width, height;



int handler(Display * d, XErrorEvent * e)
{
	printf( "XShmGetImage error\n" );
    return 0;
}

void PreRender()
{
	if( windowtrack )
	{
		XWindowAttributes attribs;
	    XGetWindowAttributes(CNFGDisplay, windowtrack, &attribs);
		int taint = 0;
		if( attribs.width != width ) taint = 1;
		if( attribs.height != height ) taint = 1;
	    width = attribs.width;
	    height = attribs.height;
	//	printf( "%d %d\n", width, height );

		if( !image )
		{
			image = XShmCreateImage( CNFGDisplay, 
				attribs.visual, attribs.depth,
				ZPixmap, NULL, &shminfo, width, height); 

			shminfo.shmid = shmget(IPC_PRIVATE,
				image->bytes_per_line * image->height,
				IPC_PRIVATE | IPC_EXCL | IPC_CREAT|0777);

			printf( "%d ERRNO %d %d %d\n", shminfo.shmid, errno, image->bytes_per_line, image->height );

			if( shminfo.shmid > 0 )
			{
				shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
				shminfo.readOnly = False;
				XShmAttach(CNFGDisplay, &shminfo);
			}
		}
		else
		if(taint )
		{
			printf( "TAINT!!\n" );
			image = XShmCreateImage( CNFGDisplay, 
				attribs.visual, attribs.depth,
				ZPixmap, NULL, &shminfo, width, height); 
			shminfo.shmid = shmget(IPC_PRIVATE,
				image->bytes_per_line * image->height,
				IPC_CREAT|0777);

			if( shminfo.shmid > 0 )
			{
				shminfo.shmaddr = image->data = shmat(shminfo.shmid, 0, 0);
				shminfo.readOnly = False;
				XShmAttach(CNFGDisplay, &shminfo);
			}
		}


		if( shminfo.shmid > 0 )
		{
			//XSetErrorHandler(handler);
			XShmGetImage(CNFGDisplay, windowtrack, image, 0, 0, AllPlanes);
			//XSetErrorHandler(0);
		//	printf( "%p\n", (void*)(&(image->data[0])) );
			if( image && image->data && (void*)(&(image->data[0])) != (void*)(-1) )
			{
				printf( "A %p %d %d %p\n", texture, image->width, image->height, (void*)(&(image->data[0])) );
				CNOVRTextureLoadDataNow( texture, image->width, image->height, 4, 0,  (void*)(&(image->data[0])), 1 );
				printf( "B\n" );
			}
		}
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
	pose.Rot[0] = -.707; pose.Rot[1] = .707;
	shader = CNOVRShaderCreate( "rendermodel" );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
//	CNOVRModelAppendMesh( model, 2, 2, 1, 1 );
	CNOVRModelAppendCube( model, (cnovr_point3d){ 1.f, 1.f, 1.f }, 0 );
//	CNOVRModelApplyTextureFromFileAsync( model, "test.png" );
	texture = CNOVRTextureCreate( 0, 0, 0 ); 
}

void ListWindows()
{
	double start = OGGetAbsoluteTime();

    bool found = false;
    Window rootWindow = RootWindow( CNFGDisplay, DefaultScreen(CNFGDisplay));
    Atom atom = XInternAtom(CNFGDisplay, "_NET_CLIENT_LIST", true);
    Atom actualType;
    int format;
    unsigned long numItems;
    unsigned long bytesAfter;
    
    unsigned char *data = '\0';
    Window *list;    
    char *windowName;

    int status = XGetWindowProperty(CNFGDisplay, rootWindow, atom, 0L, (~0L), false,
        AnyPropertyType, &actualType, &format, &numItems, &bytesAfter, &data);
    list = (Window *)data;
    
    if (status >= Success && numItems) {
        for (int i = 0; i < numItems; ++i) {
            status = XFetchName(CNFGDisplay, list[i], &windowName);
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
	    XGetWindowAttributes(CNFGDisplay, windowtrack, &attribs);

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

	printf( "Dockables start OK %s\n", identifier );
}

void stop( const char * identifier )
{
	CNOVRListAdd( cnovrLRender, &handle, Render );
	CNOVRListDeleteTag( &handle );
	printf( "Dockables Destroying: %p %p\n",shader, model );
	CNOVRDelete( shader );
	CNOVRDelete( model );
	printf( "Dockables End stop\n" );
}

