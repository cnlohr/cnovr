//This was one of the first programs written when developing cnovr.
//Do not use this as a template for "good coding" moving forward.

#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <chew.h>

#ifdef WINDOWS


#define SRCCOPY (DWORD)0x00CC0020 /* dest = source */
#define SM_CYVIRTUALSCREEN 79
#define SM_CXVIRTUALSCREEN 78
HDC GetDC(HWND hWnd);
HDC CreateCompatibleDC(HDC hdc);
int GetSystemMetrics(int nIndex);
HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HGDIOBJ SelectObject(HDC     hdc,HGDIOBJ h);
int ReleaseDC(HWND hWnd,HDC  hDC);
BOOL DeleteObject(HGDIOBJ ho);
BOOL DeleteDC(HDC hdc);
int GetWindowTextA(HWND  hWnd,LPSTR lpString,int   nMaxCount);
BOOL QueryFullProcessImageNameA(HANDLE hProcess,DWORD  dwFlags,LPSTR  lpExeName,PDWORD lpdwSize);

typedef BOOL CALLBACK WNDENUMPROC(HWND hwnd,LPARAM lParam);
BOOL EnumWindows(WNDENUMPROC lpEnumFunc,LPARAM      lParam);
/*
	//https://stackoverflow.com/questions/5069104/fastest-method-of-screen-capturing-on-windows
	hdc = GetDC(NULL); // get the desktop device context
	HDC hDest = CreateCompatibleDC(hdc); // create a device context to use yourself
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	HBITMAP hbDesktop = CreateCompatibleBitmap( hdc, width, height);
	SelectObject(hDest, hbDesktop);
	BitBlt(hDest, 0,0, width, height, hdc, 0, 0, SRCCOPY);
	ReleaseDC(NULL, hdc);
	DeleteObject(hbDesktop);
	DeleteDC(hDest);
*/
#define Window HWND

#else
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

int handle;

cnovr_shader * shader;

/////////////////////////////////////////////////////////////////////////////////
// Display stuff

og_thread_t gtt;

int frame_in_buffer = -1;
int need_texture = 1;
#ifndef WINDOWS
XShmSegmentInfo shminfo;
Display * localdisplay;
#endif
int quitting;

#define MAX_DRAGGABLE_WINDOWS 16

struct DraggableWindow
{
#ifdef WINDOWS
	HWND windowtrack;
	HDC image;
	HBITMAP hbdesktop;
	HDC hdcoutput;
#else
	Window windowtrack;
	XImage * image;
#endif
	int width, height;
	int lastwidth, lastheight;
	cnovr_model * model;
	cnovr_texture * texture;
	GLuint textureid;
	GLuint pboid;
	uint8_t * mapptr;
	cnovrfocus_capture focusblock;
	int ptrx, ptry;

	int mptrx, mptry;
	int setptr;
};


// Static components
struct staticstore
{
	int initialized;
	int windowpidof[MAX_DRAGGABLE_WINDOWS];
	cnovr_pose modelpose[MAX_DRAGGABLE_WINDOWS];
	float uniformset[MAX_DRAGGABLE_WINDOWS*4];
} * store;


struct DraggableWindow dwindows[MAX_DRAGGABLE_WINDOWS];
int current_window_check;


Window GetWindowIdBySubstring( const char * windownamematch, const char * windowmatchexename, int matchpid /*-1 to ignore*/, int * pidout );

int AllocateNewWindow( const char * name, const char * matchingwindowpname, int pidmatch )
{
	int pid;
	Window wnd = GetWindowIdBySubstring( name, matchingwindowpname, pidmatch, &pid );
	//printf( "GOT WINDOW: %d / %d\n", wnd, pid );
	if( !wnd )
	{
		printf( "Can't find window name %s\n", name );
		return -1;
	}
	int i;
	struct DraggableWindow * dw;
	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		dw = &dwindows[i];
		if( !dw->windowtrack )
		{
			break;
		}
	}
	if( i == MAX_DRAGGABLE_WINDOWS )
	{
		printf( "Can't allocate another draggable window\n" );
		return -2;
	}

	dw->windowtrack = wnd;
	dw->width = 0;
	dw->height = 0;
	dw->image = 0;
	store->windowpidof[i] = pid;
	//pose_make_identity( store->modelpose + i );

	printf( "SET INTERACTABLE: %p %p\n", dw->model, &dw->focusblock );
	CNOVRModelSetInteractable( dw->model, &dw->focusblock );
	printf( "Window Allocated\n" );
	return i;
}

#ifdef WINDOWS

void CheatMouseDown( Window window, int button ) { }

struct WindowSearchStruct
{
	const char * windownamematch;
	const char * windowmatchexename;
	int matchpid;

	HWND bestwnd;
	int bestpid;
	int w, h;
};

HWND desktopwindow;

BOOL CALLBACK WNDENUMPROCThis(HWND hwnd, LPARAM lParam)
{
	struct WindowSearchStruct * match = (struct WindowSearchStruct*)lParam;
	DWORD dwProcessId;
	char windowname[256];
	GetWindowThreadProcessId(  hwnd, &dwProcessId );
	GetWindowTextA( hwnd, windowname, sizeof( windowname )-1 );
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, dwProcessId );
	char exename[256];
	DWORD exenamesize = sizeof( exename )-1;
	if( h && QueryFullProcessImageNameA(h, 0, exename, &exenamesize ) )
	{
		//Ok
	}
	else
	{
		exename[0] = 0;
	}
    CloseHandle(h);

	char hwndclassname[256];
	GetClassName( hwnd, hwndclassname, sizeof( hwndclassname ) - 1 );

	RECT rect;
	GetWindowRect( hwnd, &rect );
	int lx = rect.right;
	int ly = rect.bottom;

	if( ( hwnd == desktopwindow && match->windownamematch && strcmp( match->windownamematch, "ROOTWINDOW" ) == 0 ) ||
		( match->windownamematch && strstr( windowname, match->windownamematch ) != 0 ) ||
		( match->windowmatchexename && strstr( exename, match->windowmatchexename ) != 0 ) ||
		( match->windowmatchexename && strstr( hwndclassname, match->windowmatchexename ) != 0 ) ||
		( match->matchpid != -1 && match->matchpid == dwProcessId ) )
	{
		if( lx * ly > match->w * match->h )
		{
			match->bestwnd = hwnd;
			match->bestpid = dwProcessId;
			match->w = lx;
			match->h = ly;
		}
	}
 
	return 1;
}

Window GetWindowIdBySubstring( const char * windownamematch, const char * windowmatchexename, int matchpid, int * pidout )
{
	if( !desktopwindow )
	{
		desktopwindow = GetDesktopWindow();
	}
	printf( "------------------------- CHECKING: %s %s %d\n", windownamematch, windowmatchexename, matchpid );
	struct WindowSearchStruct match;
	match.windownamematch = windownamematch;
	match.windowmatchexename = windowmatchexename;
	match.matchpid = matchpid;
	match.bestwnd = (HWND)-1;
	match.w = 0;
	match.h = 0;
	match.bestpid = 0;
	WNDENUMPROCThis( desktopwindow, (LPARAM)&match );
	EnumDesktopWindows(NULL, WNDENUMPROCThis, (LPARAM)&match );
	if( match.bestwnd != (HWND)-1 )
	{
		printf( "Found window %d %d %d %d\n", match.bestwnd, match.bestpid, match.w, match.h );
		*pidout = match.bestpid;
		return match.bestwnd;
	}
	else
	{
		return (HWND)0;
	}
}

#else
	
int Xerrhandler(Display * d, XErrorEvent * e)
{
	printf( "XShmGetImage error\n" );
    return 0;
}


Window GetWindowIdBySubstring( const char * windownamematch, const char * windowmatchexename, int matchpid, int * pidout )
{
	Window ret  = 0;

    Window rootWindow = RootWindow( localdisplay, DefaultScreen(localdisplay));
	if( windownamematch && strcmp( windownamematch, "ROOTWINDOW" ) == 0 )
	{
		return rootWindow;
	}
    Atom atom = XInternAtom(localdisplay, "_NET_CLIENT_LIST", true);
    Atom atomGetPid = XInternAtom(localdisplay, "_NET_WM_PID", true);
    Atom actualType;
    Atom actualTypeGetPid;
    int format;
	int formatGetPid;
    unsigned long numItems, numItems_pid;
    unsigned long bytesAfter;

    Window * list = 0;    

    int status = XGetWindowProperty(localdisplay, rootWindow, atom, 0L, (~0L), false,
        AnyPropertyType, &actualType, &format, &numItems, &bytesAfter, (char**)&list );

	uint8_t * pidata = 0;
    char * windowName = 0;
    
    if (status >= Success && numItems) {
		//printf( "There are %d windows\n", numItems );
        for (int i = 0; i < numItems; ++i) {
			if( !list[i] ) continue;
			pidata = 0;
			windowName = 0;
			unsigned long bytesAfter_pid;
         	int namestatus  = XFetchName(localdisplay, list[i], &windowName);

		 	int pidstatus = XGetWindowProperty(localdisplay, list[i], atomGetPid,
				0L, 1024, false, AnyPropertyType, &actualTypeGetPid, &formatGetPid,
				&numItems_pid, &bytesAfter_pid, (char**)&pidata );

			int pid = (pidata)?( pidata[1] * 256 + pidata[0] ) : 0;

	        if ( namestatus >= Success && windowName ) {
				//printf( "%s\n", windowName );
			}

			if( windownamematch )
			{
		        if ( namestatus >= Success && windowName ) {
					if( strstr( windowName, windownamematch ) != 0 )
					{
						goto success;
					}
		        }
			}

			if( pid == matchpid && matchpid >= 0 )
			{
				goto success;
			}

	
			char stp[128];
			char linkprop[1024];
			sprintf( stp, "/proc/%d/exe", pid );
			int rlp = readlink( stp, linkprop, sizeof( linkprop ) - 1 );

			if( rlp > 0 )
			{
				linkprop[rlp] = 0;
				if( windowmatchexename )
				{
					if( strstr( linkprop, windowmatchexename ) != 0 )
					{
						goto success;
					}
				}
			}

		    XFree(pidata);
		    XFree(windowName);
			continue;
success:
			ret = list[i];
			if( pidout ) *pidout = pid;
			XFree(pidata);
			XFree(windowName);
			break;
        }
    }
    XFree(list);

	return ret;
}

void CheatMouseDown( Window window, int button )
{
	//Use localdisplay
	//https://gist.github.com/pioz/726474
	// Create and setting up the event
	char ctspr[128];
	sprintf( ctspr, "xdotool click %d", button+1 );
	printf( "Calling: %s\n", ctspr );
	system( ctspr );
}

#endif

//void FinishedBufferEvent( void * tag, void * opaque )
//{
//	DraggableWindow * dw = (DraggableWindow*)opaque;
//	need_texture = 0;
//}

void * GetTextureThread( void * v )
{
#ifdef WINDOWS
	//No special initialization needed on Windows.
#else
	XInitThreads();
	localdisplay = XOpenDisplay(NULL);
	XSetErrorHandler(Xerrhandler);


	shminfo.shmid = shmget(IPC_PRIVATE,
		2048*4 * 2048,
		IPC_PRIVATE | IPC_EXCL | IPC_CREAT|0777);

	shminfo.shmaddr = shmat(shminfo.shmid, 0, 0);
	shminfo.readOnly = False;
	XShmAttach(localdisplay, &shminfo);
#endif

//	ListWindows();
//	AllocateNewWindow( 0, "firefox", -1 );
//	AllocateNewWindow( 0, "notepad++", -1 );
//	AllocateNewWindow( "Frame Timing", 0, -1 );
//	AllocateNewWindow( ": ~/Fonts", 0, -1 );
//	AllocateNewWindow( 0, "/xed", -1 );
	AllocateNewWindow( "ROOTWINDOW", 0, -1 );


#ifdef WINDOWS
	while( !quitting )
	{
		if( !need_texture ) { OGUSleep( 2000 ); continue; }
		current_window_check = (current_window_check+1)%MAX_DRAGGABLE_WINDOWS;
		struct DraggableWindow * dw = &dwindows[current_window_check];
		if( !dw->windowtrack ) { if( current_window_check == 0 ) OGUSleep( 2000 ); continue; }
		
		RECT rect;
		GetWindowRect( dw->windowtrack, &rect );
		int lx = rect.right - rect.left;
		int ly = rect.bottom - rect.top;

		int taint = 0;
		if( lx != dw->width ) taint = 1;
		if( ly != dw->height ) taint = 1;
		int width = dw->width = lx;
		int height = dw->height = ly;

		if( !dw->image )
		{
			taint = 1;
			dw->image = GetDC( dw->windowtrack ); // get the desktop device context
			dw->hdcoutput = CreateCompatibleDC( dw->image ); // create a device context to use yourself
			dw->hbdesktop = CreateCompatibleBitmap( dw->image , width, height);
		}
		else if( taint )
		{
			DeleteObject( dw->hbdesktop );
		}

		if( taint )
		{
			dw->hbdesktop = CreateCompatibleBitmap( dw->image , width, height);
		}
	 
	 
	 	SelectObject(dw->hdcoutput, dw->hbdesktop);
		BitBlt(dw->hdcoutput, 0,0, width, height, dw->image, 0, 0, SRCCOPY);
		BITMAPINFOHEADER bmi = {0};
		bmi.biSize = sizeof(BITMAPINFOHEADER);
		bmi.biPlanes = 1;
		bmi.biBitCount = 32;
		bmi.biWidth = width;
		bmi.biHeight = -height;
		bmi.biCompression = BI_RGB;
		bmi.biSizeImage = width * height;
		GetDIBits(dw->hdcoutput, dw->hbdesktop, 0, height, dw->mapptr, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

		POINT p;
		GetCursorPos( &p );
		{
			dw->ptrx = p.x;
			dw->ptry = p.y;
		}


		frame_in_buffer = current_window_check;

		//No way we'd need to be woken up faster than this.
		OGUSleep( 2000 );
		
		
	}
#else
	while( !quitting )
	{
		if( !need_texture ) { OGUSleep( 2000 ); continue; }
		current_window_check = (current_window_check+1)%MAX_DRAGGABLE_WINDOWS;
		struct DraggableWindow * dw = &dwindows[current_window_check];
		if( !dw->windowtrack ) { if( current_window_check == 0 ) OGUSleep( 2000 ); continue; }

		XWindowAttributes attribs;
		XGetWindowAttributes(localdisplay, dw->windowtrack, &attribs);
		int taint = 0;
		if( attribs.width != dw->width ) taint = 1;
		if( attribs.height != dw->height ) taint = 1;
		int width = dw->width = attribs.width;
		int height = dw->height = attribs.height;

		if( taint  || !dw->image)
		{
			if( dw->image ) XDestroyImage( dw->image );
			dw->image = XShmCreateImage( localdisplay, 
				attribs.visual, attribs.depth,
				ZPixmap, NULL, &shminfo, width, height); 

			dw->image->data = shminfo.shmaddr;
		}
	 
		int rx, ry, wx, wy, mask;
		Window windowreturned;
		int ptrresult = XQueryPointer( localdisplay, dw->windowtrack, &windowreturned, &windowreturned, &rx, &ry, &wx, &wy, &mask );
		if( ptrresult )
		{
			dw->ptrx = wx;
			dw->ptry = wy;
		}
//        if (ptrresult == True) {
 //           printf( "%d  %d %d %d %d %d\n", ptrresult, rx, ry, wx, wy, mask );
  //      }

	//	printf( "%d %d\n", dw->windowtrack, dw->image );
		double ds = OGGetAbsoluteTime();
		XShmGetImage(localdisplay, dw->windowtrack, dw->image, 0, 0, AllPlanes);
		double part1 = OGGetAbsoluteTime() - ds;
		//printf( "Got Frame %d %p %p %p\n", current_window_check, dw->image, dw->image->data, *(int32_t*)(&(dw->image->data[0])) );
		if( dw->image && dw->image->data && (void*)(&(dw->image->data[0])) != (void*)(-1) )
		{
			memcpy( dw->mapptr, dw->image->data, dw->width * dw->height * 4 );
			need_texture = 0;
			//CNOVRJobTack( cnovrQPrerender, FinishedBufferEvent, 0, dw, 0 );
			frame_in_buffer = current_window_check;
		}

		//Send a mouse click, if we need to.
		if( dw->setptr )
		{
		    XWarpPointer(localdisplay, None, dw->windowtrack, 0, 0, 0, 0, dw->mptrx, dw->mptry);
			if( dw->setptr > 1 )
			{
				//Based on https://www.linuxquestions.org/questions/programming-9/simulating-a-mouse-click-594576/
				printf( "Sending click %d\n", dw->setptr );
				CheatMouseDown( localdisplay, dw->windowtrack, dw->setptr-2 );
			}
			dw->setptr = 0;
		}

		//No way we'd need to be woken up faster than this.
		OGUSleep( 2000 );
	}
	printf( "Closing display\n" );
	XCloseDisplay( localdisplay );	
#endif

	printf( "Closing thraed\n" );
	return 0;
}



void PreRender()
{
}

void Update()
{
}


void PostRender()
{

	if( frame_in_buffer >= 0 )
	{
		struct DraggableWindow * dw = &dwindows[frame_in_buffer];
		if( dw->lastwidth != dw->width || dw->lastheight != dw->height )
		{
			cnovr_vbo * geo = dw->model->pGeos[0];
			cnovr_vbo * geoextra = dw->model->pGeos[3];
			int x, y, side;
			int rows = 1;
			int cols = 1;
			float aspect = dw->width / ((float)dw->height + 1.0);
			for( side = 0; side < 2; side++ )
			for( y = 0; y <= rows; y++ )
			for( x = 0; x <= cols; x++ )
			{
				float * stage = &geo->pVertices[((x + y*2) * 3)+side*12];
				stage[0] = aspect * (side?(x/(float)rows):(1-x/(float)rows));
				stage[1] = y/(float)cols;
				stage[0] = (stage[0] - 0.5)*2.0;
				stage[1] = (stage[1] - 0.5)*2.0;
				stage[2] = (side)*0.1 - 0.05;

				stage = &geoextra->pVertices[((x + y*2) * 4)+side*16];
				//printf( "Extra: %f %f %f %f %f\n", stage[0], stage[1], stage[2], stage[3] );
				CNOVRVBOTaint( geo );
//				CNOVRVBOTaint( geoextra );
				dw->lastwidth = dw->width;
				dw->lastheight = dw->height;
			}
			glBindTexture( GL_TEXTURE_2D, dw->textureid );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, dw->width, dw->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
		} 
		//CNOVRTextureLoadDataNow( dw->texture, dw->width, dw->height, 4, 0, dw->mapptr, 1 );

		//glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dw->width, dw->height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)(&(dw->image->data[0])) );

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, dw->pboid ); 
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER );

		glBindTexture( GL_TEXTURE_2D, dw->textureid );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, dw->width, dw->height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glGenerateMipmapCHEW(GL_TEXTURE_2D);
		glBindTexture( GL_TEXTURE_2D, 0 );
		dw->mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, 2048*2048*4, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
//		dw->mapptr = glMapBuffer( GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY );
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 ); 


		store->uniformset[frame_in_buffer*4+0] = dw->width;
		store->uniformset[frame_in_buffer*4+1] = dw->height;
		need_texture = 1;
		frame_in_buffer = -1;
	}
}

void Render()
{
	int i;
	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		struct DraggableWindow * dw = &dwindows[i];
		//float aspect = dw->width / ((float)dw->height + 1.0);
		if( dw->width > 0 && dw->height > 0 )
		{
			store->uniformset[i*4+2] = dw->ptrx / (float)dw->width;
			store->uniformset[i*4+3] = dw->ptry / (float)dw->height;
		}
	}
	CNOVRRender( shader );
	glUniform4fv( 19, MAX_DRAGGABLE_WINDOWS*4, store->uniformset );
	glGetError(); //glGetError ignores the error if we aren't examining uniform #19
	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		struct DraggableWindow * dw = &dwindows[i];
		if( dw->windowtrack && store->windowpidof[i] >= 0 )
		{
			glBindTexture( GL_TEXTURE_2D, dw->textureid );
			//CNOVRRender( dw->texture );
			CNOVRRender( dw->model );
		}
	}
}


int DockableWindowFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	cnovr_model * m = (cnovr_model*)cap->opaque;
//	printf( "Event: %d  %f %f %f %d\n", event, PFTHREE( prop->NewPassiveProps ), buttoninfo );

	if( event == CNOVRF_MOTION || event == CNOVRF_DOWNNOFOCUS || event == CNOVRF_DOWNFOCUS )
	{
		int i;
		struct DraggableWindow * dw = 0;
		for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
		{
			if( dwindows[i].model == m ) { dw = &dwindows[i]; break; }
		}
		if( dw && !dw->setptr )
		{
			dw->mptrx = (1.0-prop->NewPassiveProps[0]) * dw->width;
			dw->mptry = (prop->NewPassiveProps[1]) * dw->height;
			dw->setptr = (event == CNOVRF_MOTION)?1:(buttoninfo+2);
		}
	}


	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo );
	if( event == CNOVRF_LOSTFOCUS )
	{
		CNOVRNamedPtrSave( "draggablewindowsdata" );
	}
	return 0;
}



void prerender_startup( void * tag, void * opaquev )
{
	printf( "=== Prerender startup\n" );
	int i;
	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		struct DraggableWindow * dw = &dwindows[i];

		//XXX TODO: Wouldn't it be cool if we could make this a single render call?
		//Not sure how we would handle the textures, though.
		//XXX JUST FYI!!! All of this gets blown away on the first render.  This is more just for allocating the space.
		dw->model = CNOVRModelCreate( 0, GL_TRIANGLES );
		cnovr_point4d extradata = { i, 0, 0, 0 };
		CNOVRModelAppendMesh( dw->model, 1, 1, 1, (cnovr_point3d){ 1, 1, 0 }, 0, &extradata );
		CNOVRModelAppendMesh( dw->model, 1, 1, 1, (cnovr_point3d){ -1, 1, 0. }, 0, &extradata );
		if( !store->initialized )
		{
			pose_make_identity( store->modelpose + i );
			store->windowpidof[i] = -1;
		}
		dw->model->pose = store->modelpose + i;
		dw->focusblock.tag = 0;
		dw->focusblock.opaque = dw->model;
		dw->focusblock.cb = DockableWindowFocusEvent;
		dw->texture = CNOVRTextureCreate( 0, 0, 0 );
		CNOVRTextureLoadDataNow( dw->texture, 1, 1, 4, 0, "xxxx", 1 );
		dw->textureid = dw->texture->nTextureId;
		glBindTexture( GL_TEXTURE_2D, dw->textureid );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

		glGenBuffers( 1, &dw->pboid );
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, dw->pboid ); //bind pbo
		glBufferData(GL_PIXEL_UNPACK_BUFFER, 2048*2048*4, NULL, GL_STREAM_DRAW);
		dw->mapptr = glMapBufferRange( GL_PIXEL_UNPACK_BUFFER, 0, 2048*2048*4, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	}

	store->initialized = 1;

	gtt = OGCreateThread( GetTextureThread, 0 );

	shader = CNOVRShaderCreate( "draggablewindow" );

	CNOVRListAdd( cnovrLRender2, &handle, Render );
	CNOVRListAdd( cnovrLPrerender, &handle, PreRender );
	CNOVRListAdd( cnovrLUpdate, &handle, Update );
	CNOVRListAdd( cnovrLPostRender, &handle, PostRender );
	printf( "=== Prerender startup complete\n" );
}

void start( const char * identifier )
{
	printf( "=== draggablewindows.c Start\n" );
	store = CNOVRNamedPtrData( "draggablewindowsdata", 0, 1024 );
//	store->initialized = 0;

	printf( "=== Store: %p\n", store );
	printf( "=== Dockables start %s(%p)\n", identifier, identifier );
	CNOVRJobTack( cnovrQPrerender, prerender_startup, 0, 0, 0 );
	printf( "=== Dockables start OK %s\n", identifier );

}

void stop( const char * identifier )
{
	printf( "=== draggablewindows.c stop\n" );
	CNOVRListDeleteTCCTag( 0 );

	quitting = 1;
	if( gtt ) 
	{
		printf( "=== actually joining\n" );
		OGJoinThread( gtt );
	}
	printf( "=== Stopped\n" );

	CNOVRListDeleteTag( &handle );
	printf( "=== Dockables Destroying: %p %p\n" ,shader );
	int i;
	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		struct DraggableWindow * dw = &dwindows[i];
		CNOVRDelete( dw->model );
		CNOVRDelete( dw->texture );
	}
	
#ifdef WINDOWS

	for( i = 0; i < MAX_DRAGGABLE_WINDOWS; i++ )
	{
		struct DraggableWindow * dw = &dwindows[i];
		ReleaseDC(NULL, dw->image);
		DeleteObject(dw->hbdesktop);
		DeleteDC(dw->hdcoutput);
	}

#else
	//Freeing shmem
	if( shminfo.shmid >= 0 )
	{
		shmdt(shminfo.shmaddr);
		/* 'remove' shared memory segment */
		shmctl(shminfo.shmid, IPC_RMID, NULL);
	}
#endif

	CNOVRDelete( shader );
	printf( "=== Dockables End stop\n" );
}

