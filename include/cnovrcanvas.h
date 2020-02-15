#ifndef _CNOVR_CANVAS_H
#define _CNOVR_CANVAS_H

#include "cnovrparts.h"
#include "cnovrfocus.h"

struct cnovr_canvas_t;
struct cnovr_canvas_canned_gui_element_t;

typedef void (*canvas_canned_gui_cb)( struct cnovr_canvas_t * canvas, const struct cnovr_canvas_canned_gui_element_t * element, int rx, int ry, cnovrfocus_event event, int devid );

typedef struct cnovr_canvas_canned_gui_element_t
{
	int w, h;
	int x, y;
	canvas_canned_gui_cb cb;
	const char * text;
	const void * vopaque;
	int iopaque;
	int allowdrag;
	int * disabled;
} cnovr_canvas_canned_gui_element;

typedef struct cnovr_canvas_t
{
	cnovr_base base;
	
	//Default behavior is to CPU-blit the texture, since that lets it be used from other threads.
	uint32_t color;
	uint32_t bgcolor;
	int w;
	int h;
	uint32_t * data;
	int linewidth;
	cnovr_model * model;
	cnovr_pose  * pose;
	cnovr_shader * shd;
	cnovr_shader * overrideshd;
	cnovrfocus_capture capture;
	const cnovr_canvas_canned_gui_element * pCannedGUI;
	char * canvasname;
	int set_filter_type;
	float presw;
	float presh;
	int iOpaque;
} cnovr_canvas;

#define CANVAS_PROP_NO_INTERACT 1

//Tricky:  If you want to use this in some advanced way, abusing the model/texture, you can create a model that is w=1, h=1
cnovr_canvas * CNOVRCanvasCreate( const char * name, int w, int h, int properties );
void CNOVRCanvasApplyCannedGUI( cnovr_canvas * c, const cnovr_canvas_canned_gui_element * canned ); //Applies, or re-renders canned GUI.
void CNOVRCanvasResize( cnovr_canvas * c, int w, int h );
void CNOVRCanvasSetPhysicalSize( cnovr_canvas * c, float sx, float sy );
void CNOVRCanvasYFlip( cnovr_canvas * c, int yes_flip_y );
void CNOVRCanvasTackPixel( cnovr_canvas * c, int x, int y );
void CNOVRCanvasDrawText( cnovr_canvas * c, int x, int y, const char * text, int scale );
void CNOVRCanvasTackSegment( cnovr_canvas * c, int x1, int y1, int x2, int y2 );
void CNOVRCanvasDrawBox( cnovr_canvas * c, int x1, int y1, int x2, int y2 );
void CNOVRCanvasTackRectangle( cnovr_canvas * c, int x1, int y1, int x2, int y2 ); //Uses foreground
#define CNOVRCanvasColor( c, col ) c->color = c
#define CNOVRCanvasBGColor( c, col ) c->bgcolor = c
void CNOVRCanvasTackPoly( cnovr_canvas * c, int * points, int verts );
void CNOVRCanvasSwapBuffers( cnovr_canvas * c );
void CNOVRCanvasClearFrame( cnovr_canvas * c ); //Uses background color.
#define CNOVRCanvasSetLineWidth( c, wid ) c->linewidth = wid

//You can use this function from CNFG as an aid.
//void CNFGGetTextExtents( const char * text, int * w, int * h, int textsize )


#endif

