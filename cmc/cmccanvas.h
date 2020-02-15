cnovr_canvas * canvas;
float fpstime = 1.;

void UpdateCanvas( )
{
    int i;
	static double start;
	static double last;
	static double lastframetime;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { start = now; last = now; return; }
	double runtime = now - start;
	float deltatime = (float)(now - last);
	last = now;

	CNOVRCanvasClearFrame( canvas );
	static int frameno;
	frameno++;
	CNOVRCanvasSetLineWidth( canvas, 1 );
	if( (frameno % 10) == 0 )
	{
		fpstime = (float)(now-lastframetime);
		lastframetime = now;
	}
	static float ovrhist[96];
	static int histhead;
	ovrhist[histhead] = cnovrstate->fFrameTimems;
	histhead = (histhead+1)%96;
	CNOVRCanvasDrawText( canvas, 2, 2, trprintf( "%3.fFPS\n%4d\n%.2fms", 10./fpstime, store->points, cnovrstate->fFrameTimems ), 3 );
	for( i = 0; i < 96; i++ )
	{
		int px = ovrhist[(i+histhead)%96]*2.0; 
		canvas->color = 0xff00ff00;
		if( px > 18 ) canvas->color = 0xff0000ff;
		CNOVRCanvasTackSegment( canvas, i, 64, i, 64-px );
	}
	canvas->color = 0xffffffff;
	CNOVRCanvasSwapBuffers( canvas );
}

void RenderCanvas()
{
	CNOVRRender( canvas );
}

void InitCanvas()
{
	canvas = CNOVRCanvasCreate( "ExampleCanvas", 96, 64, 0 );
}

