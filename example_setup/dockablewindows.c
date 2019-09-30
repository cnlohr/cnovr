#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

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
cnovr_pose pose;

void Render()
{
	CNOVRRender( shader );
	CNOVRModelRenderWithPose( model, &pose );
}

void GenGeo()
{
	printf( "Genning\n" );
	pose_make_identity( &pose );
	shader = CNOVRShaderCreate( "rendermodel" );
	model = CNOVRModelCreate( 0, 3, GL_TRIANGLES );
	CNOVRModelAppendMesh( model, 2, 2, 1, 1 );
	CNOVRModelApplyTextureFromFileAsync( model, "test.png" );
}

void ListWindows()
{
	double start = OGGetAbsoluteTime();

	Window windowret;
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
					windowret = list[i];
					found = 1;
					break;
				}
                //string windowNameStr(windowName);
                //if (windowNameStr.find(WINDOW_DOXBOX) == 0) {
                 //   window = list[i];
                 //   found = true;
                 //   break;
                //}
            }
        }
    }
    XFree(windowName);
    XFree(data);

	if( found )
	{
		//windowret
		XWindowAttributes attribs;
	    XGetWindowAttributes(CNFGDisplay, windowret, &attribs);

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
}

void start( const char * identifier )
{
	printf( "Example start %s(%p)\n", identifier, identifier );

	CNOVRJobTack( cnovrQPrerender, prerender_startup, 0, 0, 0 );

	printf( "Example start OK %s\n", identifier );
}

void stop( const char * identifier )
{
	CNOVRListAdd( cnovrLRender, &handle, Render );
	CNOVRListDeleteTag( &handle );
	printf( "Destroying: %p %p\n",shader, model );
	CNOVRDelete( shader );
	CNOVRDelete( model );
	printf( "End stop\n" );
}

