cnovr_canvas * titlecanvas;
cnovr_canvas * scorecanvas;
//float fpstime = 1.;

#define UI_WINDOW_COLOR 0x00004DFF
#define UI_TEXT_COLOR 0x00000000

//overballstore_t
void InitUI()
{
	//CANVAS_PROP_NO_INTERACT
	titlecanvas = CNOVRCanvasCreate( "titlecanvas", 260, 152, CANVAS_PROP_NO_INTERACT );
	scorecanvas = CNOVRCanvasCreate( "scorecanvas", 200, 40, CANVAS_PROP_NO_INTERACT );
}

void UpdateUI( float deltatime )
{
	// time calculations
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	last = now;

	//store->points // current points

	//0x0000GGRR
	titlecanvas->bgcolor = UI_WINDOW_COLOR;
	titlecanvas->color = UI_TEXT_COLOR;
	
	scorecanvas->bgcolor = UI_WINDOW_COLOR;
	scorecanvas->color = UI_TEXT_COLOR;
	
	//cnovrfocus_properties * headset_focus = CNOVRFocusGetPropsForDev( int ctrl );
	//titlecanvas->pose = cnovrfocus_properties * CNOVRFocusGetPropsForDev( int ctrl )
	//scorecanvas->pose = 
	
	// Get headset properties 
	cnovrfocus_properties * p = CNOVRFocusGetPropsForDev( 0 );
	//titlecanvas->pose = p.poseTip;
	// TODO: from p->tip, project outwards and at a rotation to the side.
	
	/*
	typedef struct cnovr_pose {
		cnovr_quat Rot;
		cnovr_point3d Pos;
		FLT Scale;
	} cnovr_pose;
	*/

	// clear canvas
	CNOVRCanvasClearFrame( titlecanvas );
	CNOVRCanvasClearFrame( scorecanvas );
	
	CNOVRCanvasSetLineWidth( titlecanvas, 3 );
	CNOVRCanvasSetLineWidth( scorecanvas, 3 );
	
	CNOVRCanvasDrawText( titlecanvas, 2, 4, trprintf( "  COSMIC\n MISCREANT\nCONDITIONER" ), 8 );
	CNOVRCanvasDrawText( scorecanvas, 2, 4, trprintf( "SCORE: %04d", store->points ), 6 );
	
	CNOVRCanvasSwapBuffers( titlecanvas );
	CNOVRCanvasSwapBuffers( scorecanvas );
}

void RenderUI()
{
	CNOVRRender( titlecanvas );
	CNOVRRender( scorecanvas );
}