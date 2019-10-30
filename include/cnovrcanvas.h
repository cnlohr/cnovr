#ifndef _CNOVR_CANVAS_H
#define _CNOVR_CANVAS_H

#include "cnovrparts.h"

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
	cnovr_pose    pose;
	float presw;
	float presh;
	int iOpaque;
} cnovr_canvas;

//Tricky:  If you want to use this in some advanced way, abusing the model/texture, you can create a model that is w=1, h=1

cnovr_canvas * CNOVRCanvasCreate( int w, int h );
void CNOVRCanvasResize( cnovr_canvas * c, int w, int h );
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

