cnovr_canvas * uicanvas;
//float fpstime = 1.;

//overballstore_t

void UpdateUI( float deltatime )
{
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	last = now;

	//0x00000000
	uicanvas->bgcolor = 0x000000FF;

	// clear canvas
	CNOVRCanvasClearFrame( uicanvas );
	
	static double lastframetime;
	static int frameno;
	frameno++;
	if( (frameno % 10) == 0 )
	{
		//fpstime = (float)(now-lastframetime);
		lastframetime = now;
	}
	CNOVRCanvasSetLineWidth( uicanvas, 1 );
	CNOVRCanvasDrawText( uicanvas, 2, 2, trprintf( "COSMIC\nMISCREANT\nCONDITIONER"), 8 );//
	/*static float ovrhist[96];
	static int histhead;
	ovrhist[histhead] = cnovrstate->fFrameTimems;
	histhead = (histhead+1)%96;*/
	//CNOVRCanvasDrawText( uicanvas, 2, 2, trprintf( "%3.fFPS\n%4d\n%.2fms", 10./fpstime, points, cnovrstate->fFrameTimems ), 3 );
	/*for( i = 0; i < 96; i++ )
	{
		//int px = ovrhist[(i+histhead)%96]*2.0; 
		uicanvas->color = 0xff00ff00;
		//if( px > 18 ) uicanvas->color = 0xff0000ff;
		CNOVRCanvasTackSegment( uicanvas, i, 64, i, 64-px );
	}*/
	//??BBGGRR
	//FFA500
	uicanvas->color = 0x000000FF;
	//CNOVRCanvasBGColor(uicanvas, 0x0000FF00);
	CNOVRCanvasSwapBuffers( uicanvas );
}

void RenderUI()
{
	CNOVRRender( uicanvas );
}

void InitUI() //this
{
	uicanvas = CNOVRCanvasCreate( "UICanvas", 260, 150, 0 );
}

