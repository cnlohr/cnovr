#include "cnovrcanvas.h"
#include "cnovrtccinterface.h"
#include "cnovrutil.h"
#include "cnovr.h"
#include <string.h>
#include <stdlib.h>

extern const unsigned short FontCharMap[];
extern const unsigned char FontCharData[];


static void CNOVRCanvasDelete( cnovr_canvas * ths )
{
	CNOVRDelete( ths->shd );
	CNOVRDelete( ths->model );
	free( ths->canvasname );
//	CNOVRFreeLater( ths->data );  //Free'd by the texture.
	CNOVRFreeLater( ths );
}

static void CNOVRCanvasRender( cnovr_canvas * ths )
{
	if( ths->overrideshd )
		CNOVRRender( ths->overrideshd );
	else
		CNOVRRender( ths->shd );

	if( ths->set_filter_type == 0 && ths->model && ths->model->pTextures && ths->model->pTextures[0]->nTextureId )
	{
		glBindTexture( GL_TEXTURE_2D, ths->model->pTextures[0]->nTextureId );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glBindTexture( GL_TEXTURE_2D, 0 );
		ths->set_filter_type = 1;
	}
	CNOVRRender( ths->model );
}

cnovr_header cnovr_canvas_header = {
	(cnovrfn)CNOVRCanvasDelete, //Delete
	(cnovrfn)CNOVRCanvasRender, //Render
	TYPE_CANVAS,
};

static int CanvasFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	cnovr_canvas * c = (cnovr_canvas*)cap->opaque;
	cnovr_model * m = c->model;
	int id = m->iOpaque;

	if( event == CNOVRF_LOSTFOCUS )
	{
		CNOVRNamedPtrSave( c->canvasname );
	}
	if( ( event == CNOVRF_DOWNFOCUS || event == CNOVRF_DOWNNOFOCUS ) && ( c->pCannedGUI ) && buttoninfo == CTRLA_TRIGGER )
	{
		int hx = (int)(prop->NewPassiveProps[0] * c->w);
		int hy = (int)(prop->NewPassiveProps[1] * c->h);
		const cnovr_canvas_canned_gui_element * curcan = c->pCannedGUI;
		while( curcan->w != 0 || curcan->h != 0 )
		{
			int x = curcan->x;
			int y = curcan->y;
			int w = curcan->w;
			int h = curcan->h;

			if( curcan->cb && hx >= x && hx <= x + w && hy >= y && hy <= y + h )
			{
				curcan->cb( c, curcan->iopaque, hx-x, hy-y, event );
			}
			curcan++;
		}
	}

//	if( event == CNOVRF_DOWNNOFOCUS && buttoninfo == CTRLA_TRIGGER )
	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );
	return 0;
}


cnovr_canvas * CNOVRCanvasCreate( const char * name, int w, int h )
{
	cnovr_canvas * ret = malloc( sizeof( cnovr_canvas ) );
	
	ret->base.header = &cnovr_canvas_header;
	ret->base.tccctx = TCCGetTag();
	ret->color = 0xffffffff;
	ret->bgcolor = 0xff801010;
	ret->w = w;
	ret->h = h;
	ret->presw = 1;
	ret->presh = ((float)h)/((float)w);
	ret->iOpaque = 0;
	ret->pCannedGUI = 0;
	ret->overrideshd = 0;
	ret->data = malloc( w * h * 4 );
	ret->linewidth = 1;
	ret->set_filter_type = 0;
	ret->canvasname = strdup( trprintf( "%s_pose", name ) );
	ret->pose = CNOVRNamedPtrData( ret->canvasname, "cnovr_pose", sizeof( cnovr_pose ) );

	ret->model = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point4d extradata = { 0, 0, 0, 0 };
	CNOVRModelAppendMesh( ret->model, 1, 1, 1, (cnovr_point3d){  ret->presw/2, ret->presh/2, .001 }, 0, &extradata );
	CNOVRModelAppendMesh( ret->model, 1, 1, 1, (cnovr_point3d){ -ret->presw/2, ret->presh/2, -.001 }, 0, &extradata );
	if( ret->pose->Scale == 0 ) pose_make_identity( ret->pose );
	ret->model->iOpaque = -1;
	ret->model->pose = ret->pose;
	CNOVRModelSetNumTextures( ret->model, 1 );

	ret->capture.tcctag = TCCGetTag();
	ret->capture.tag = 0;
	ret->capture.opaque = ret;
	ret->capture.cb = CanvasFocusEvent;
	CNOVRModelSetInteractable( ret->model, &ret->capture );

	

//	glBind
//	for( i = m->iTextures; i < textures; i++ )
//		m->pTextures[i] = CNOVRTextureCreate( 1, 1, 4 );

	ret->shd = CNOVRShaderCreate( "rendermodel" );
	//We have our model, but nothing applied.
	//CNOVRModelApplyTextureFromFileAsync( ret->model, "picturealbum/container.png" );

	return ret;
}

void CNOVRCanvasResize( cnovr_canvas * c, int w, int h )
{
	uint32_t * rmap = malloc( w * h * 4 );
	int x, y;
	int icw = c->w;
	int ch = (h<c->h)?h:c->h;
	int cw = (w<icw)?w:icw;
	uint32_t cfgcolor = c->color;
	for( y = 0; y < ch; y++ )
	{
		uint32_t * scanlinestart = rmap + y * w;
		memcpy( scanlinestart, c->data + icw * y, 4 * cw );
		for( x = icw; x < w; x++ )
			scanlinestart[x] = cfgcolor;
	}
	for( ; y < h; y++ )
	{
		uint32_t * scanlinestart = rmap + y * w;
		for( x = 0; x < w; x++ )
			scanlinestart[x] = cfgcolor;
	}
	CNOVRFreeLater( c->data );
	c->w = w;
	c->h = h;
	c->data = rmap;
}

void CNOVRCanvasApplyCannedGUI( cnovr_canvas * c, const cnovr_canvas_canned_gui_element * canned )
{
	c->pCannedGUI = canned;
	CNOVRCanvasClearFrame( c );
	CNOVRCanvasSetLineWidth( c, 1 );
	const cnovr_canvas_canned_gui_element * curcan = canned;
	while( curcan->w != 0 || curcan->h != 0 )
	{
		int x = curcan->x;
		int y = curcan->y;
		CNOVRCanvasDrawBox( c, x, y, x + curcan->w, y + curcan->h );
		CNOVRCanvasDrawText( c, x+2, y+2, curcan->text, 2 );
		curcan++;
	}
	c->color = 0xffffffff;
	CNOVRCanvasSwapBuffers( c );
}

void CNOVRCanvasSetPhysicalSize( cnovr_canvas * c, float sx, float sy )
{
	//if( !c || !c->model || !c->model->pGeos || !c->model->pGeos[0] || !c->model->pGeos[0]->pVertices ) return;
	sx /= 2;
	sy /= 2;
	float * pf = c->model->pGeos[0]->pVertices;
	pf[0] = pf[6] = pf[15] = pf[21] = -sx;
	pf[3] = pf[9] = pf[12] = pf[18] = sx;
	pf[1] = pf[4] = pf[13] = pf[16] = -sy;
	pf[7] = pf[10] = pf[19] = pf[22] = sy;
	CNOVRVBOTaint( c->model->pGeos[0] );
//	CNOVRModelClearMeshes( c->model );
//	cnovr_point4d extradata = { 0, 0, 0, 0 };
//	CNOVRModelAppendMesh( c->model, 1, 1, 1, (cnovr_point3d){  aspect_ratio, 1, 0 }, 0, &extradata );
//	CNOVRModelAppendMesh( c->model, 1, 1, 1, (cnovr_point3d){ -aspect_ratio, 1, 0 }, 0, &extradata );
}

void CNOVRCanvasYFlip( cnovr_canvas * c, int yes_flip_y )
{
	if( !c || !c->model || !c->model->pGeos || !c->model->pGeos[1] || !c->model->pGeos[1]->pVertices ) return;
	float * pf = c->model->pGeos[1]->pVertices;
	float top = yes_flip_y?0.0:1.0;
	float bot = yes_flip_y?1.0:0.0;
	pf[4] = pf[1] = pf[13] = pf[16] = top;
	pf[7] = pf[10] = pf[19] = pf[22] = bot;
	CNOVRVBOTaint( c->model->pGeos[1] );
}


void CNOVRCanvasTackPixel( cnovr_canvas * c, int x, int y )
{
	int cw = c->w;
	int ch = c->h;
	int px, py;
	int lwa = c->linewidth;
	int lwb = lwa/2 + (lwa&1)?1:0;
	lwa /= 2;
	int minx = x - lwa;
	int maxx = x + lwb;
	int miny = y - lwa;
	int maxy = y + lwb;
	uint32_t cfgcolor = c->color;
	uint32_t * data = c->data;
	if( minx < 0 ) minx = 0;
	if( miny < 0 ) miny = 0;
	if( maxx >= cw ) maxx = cw-1;
	if( maxy >= ch ) maxy = ch-1;
//	printf( "%d %d -> %d %d\n", minx, miny, maxx, maxy );
	for( py = miny; py < maxy; py++ )
	{
		uint32_t * pd = data + py * cw + minx;
		for( px = minx; px < maxx; px++ )
		{
			(*pd++) = cfgcolor;
		}
	}
}

void CNOVRCanvasDrawText( cnovr_canvas * c, int x, int y, const char * text, int scale )
{
	const unsigned char * lmap;
	float iox = (float)x;
	float ioy = (float)y;

	int place = 0;
	unsigned short index;
	int bQuit = 0;
	while( text[place] )
	{
		unsigned char ch = text[place];

		switch( ch )
		{
		case 9: // tab
			iox += 12 * scale;
			break;
		case 10: // linefeed
			iox = (float)x;
			ioy += 6 * scale;
			break;
		default:
			index = FontCharMap[ch];
			if( index == 65535 )
			{
				iox += 3 * scale;
				break;
			}

			lmap = &FontCharData[index];
			do
			{
				int x1 = (int)((((*lmap) & 0x70)>>4)*scale + iox);
				int y1 = (int)(((*lmap) & 0x0f)*scale + ioy);
				int x2 = (int)((((*(lmap+1)) & 0x70)>>4)*scale + iox);
				int y2 = (int)(((*(lmap+1)) & 0x0f)*scale + ioy);
				lmap++;
				CNOVRCanvasTackSegment( c, x1, y1, x2, y2 );
				bQuit = *lmap & 0x80;
				lmap++;
			} while( !bQuit );

			iox += 3 * scale;
		}
		place++;
	}
}

void CNOVRCanvasDrawBox( cnovr_canvas * c, int x1, int y1, int x2, int y2 )
{
	uint32_t backupcolor = c->color;
	c->color = c->bgcolor;
	CNOVRCanvasTackRectangle( c, x1, y1, x2, y2 );
	c->color = backupcolor;
	CNOVRCanvasTackSegment( c, x1, y1, x2, y1 );
	CNOVRCanvasTackSegment( c, x2, y1, x2, y2 );
	CNOVRCanvasTackSegment( c, x2, y2, x1, y2 );
	CNOVRCanvasTackSegment( c, x1, y2, x1, y1 );
}

void CNOVRCanvasTackSegment( cnovr_canvas * c, int x1, int y1, int x2, int y2 )
{
	int tx, ty;
	float slope, lp;

	int dx = x2 - x1;
	int dy = y2 - y1;

	uint32_t * buffer = c->data;
	uint32_t color = c->color;
	if( !buffer ) return;

	if( dx < 0 ) dx = -dx;
	if( dy < 0 ) dy = -dy;

	int lwa = c->linewidth;
	int lwb = lwa/2 + (lwa&1)?1:0;
	lwa /= 2;
	
	int bufferx = c->w;
	int buffery = c->h;
	
	if( c->linewidth == 1 )
	{
		if( dx > dy )
		{
			int minx = (x1 < x2)?x1:x2;
			int maxx = (x1 < x2)?x2:x1;
			int miny = (x1 < x2)?y1:y2;
			int maxy = (x1 < x2)?y2:y1;
			float thisy = miny;
			int ly;
			slope = (float)(maxy-miny) / (float)(maxx-minx);
			if( maxx >= c->w ) maxx = c->w-1;
			for( tx = minx; tx <= maxx; tx++ )
			{
				ty = thisy;
				if( tx < 0 || ty < 0 || ty >= buffery ) continue;
				buffer[ty * bufferx + tx] = color;
				thisy += slope;
			}
		}
		else
		{
			int minx = (y1 < y2)?x1:x2;
			int maxx = (y1 < y2)?x2:x1;
			int miny = (y1 < y2)?y1:y2;
			int maxy = (y1 < y2)?y2:y1;
			float thisx = minx;
			slope = (float)(maxx-minx) / (float)(maxy-miny);
			if( maxy >= c->h ) maxy = c->h-1;

			for( ty = miny; ty <= maxy; ty++ )
			{
				tx = thisx;
				if( ty < 0 || tx < 0 || tx >= bufferx ) continue;
				buffer[ty * bufferx + tx] = color;
				thisx += slope;
			}
		}
	}
	else
	{
		if( dx > dy )
		{
			int minx = (x1 < x2)?x1:x2;
			int maxx = (x1 < x2)?x2:x1;
			int miny = (x1 < x2)?y1:y2;
			int maxy = (x1 < x2)?y2:y1;
			float thisy = miny;
			int ly;
			slope = (float)(maxy-miny) / (float)(maxx-minx);
			if( maxx >= c->w ) maxx = c->w-1;

			for( tx = minx; tx <= maxx; tx++ )
			{
				ty = thisy;
				int lx = tx - lwa;
				int lmx = tx + lwb;
				int ly = ty - lwa;
				int lmy = ty + lwb;
				
				if( lx < 0 ) lx = 0;
				if( lmx >= bufferx ) lmx = bufferx-1;
				if( ly < 0 ) ly = 0;
				if( lmy >= buffery ) lmy = buffery-1;

				for( ; ly <= lmy; ly++ )
				{
					uint32_t * bl = buffer + ly * bufferx;
					int x = lx;
					for( ; x <= lmx; x++ )
						bl[x] = color;
				}

				thisy += slope;
			}
		}
		else
		{
			int minx = (y1 < y2)?x1:x2;
			int maxx = (y1 < y2)?x2:x1;
			int miny = (y1 < y2)?y1:y2;
			int maxy = (y1 < y2)?y2:y1;
			float thisx = minx;
			slope = (float)(maxx-minx) / (float)(maxy-miny);
			if( maxy >= c->h ) maxy = c->h-1;

			for( ty = miny; ty <= maxy; ty++ )
			{
				tx = thisx;
				
				int lx = tx - lwa;
				int lmx = tx + lwb;
				int ly = ty - lwa;
				int lmy = ty + lwb;
				if( lx < 0 ) lx = 0;
				if( lmx >= bufferx ) lmx = bufferx-1;
				if( ly < 0 ) ly = 0;
				if( lmy >= buffery ) lmy = buffery-1;

				for( ; ly <= lmy; ly++ )
				{
					uint32_t * bl = buffer + ly * bufferx;
					int x = lx;
					for( ; x <= lmx; x++ )
						bl[x] = color;
				}

				thisx += slope;
			}
		}
		CNOVRCanvasTackPixel( c, x1, y1 );
		CNOVRCanvasTackPixel( c, x2, y2 );
	}
}

void CNOVRCanvasTackRectangle( cnovr_canvas * c, int x1, int y1, int x2, int y2 )
{
	int w = c->w;
	int h = c->h;
	int minx = (y1 < y2)?x1:x2;
	int maxx = (y1 < y2)?x2:x1;
	int miny = (y1 < y2)?y1:y2;
	int maxy = (y1 < y2)?y2:y1;
	int x, y;
	uint32_t * buf = c->data;
	uint32_t col = c->color;
	if( minx < 0 ) minx = 0;
	if( maxx >= w ) maxx = w-1;
	if( miny < 0 ) miny = 0;
	if( maxy >= h ) maxy = h-1;
	for( y = miny; y <= maxy; y++ )
	{
		uint32_t * bl = buf + y * w + minx;
		for( x = minx; x <= maxx; x++ )
		{
			*(bl++) = col;
		}
	}
		
}

void CNOVRCanvasTackPoly( cnovr_canvas * c, int * points, int verts )
{
	int bufferx = c->w;
	int buffery = c->h;
	uint32_t * buffer = c->data;
	
	int minx = 10000, miny = 10000;
	int maxx =-10000, maxy =-10000;
	int i, x, y;
	uint32_t color = c->color;
	
	//Just in case...
	if( verts > 32767 ) return;

	for( i = 0; i < verts; i++ )
	{
		int * p = &points[i*2];
		if( p[0] < minx ) minx = p[0];
		if( p[1] < miny ) miny = p[1];
		if( p[0] > maxx ) maxx = p[0];
		if( p[1] > maxy ) maxy = p[1];
	}

	if( miny < 0 ) miny = 0;
	if( maxy >= buffery ) maxy = buffery-1;

	for( y = miny; y <= maxy; y++ )
	{
		int startfillx = maxx;
		int endfillx = minx;

		//Figure out what line segments intersect this line.
		for( i = 0; i < verts; i++ )
		{
			int pl = i + 1;
			if( pl == verts ) pl = 0;

			int ptop[2];
			int pbot[2];

			ptop[0] = points[i*2+0];
			ptop[1] = points[i*2+1];
			pbot[0] = points[pl*2+0];
			pbot[1] = points[pl*2+1];
//printf( "Poly: %d %d\n", pbot[1], ptop[1] );

			if( pbot[1] < ptop[1] )
			{
				int ptmp[2];
				ptmp[0] = pbot[0];
				ptmp[1] = pbot[1];
				pbot[0] = ptop[0];
				pbot[1] = ptop[1];
				ptop[0] = ptmp[0];
				ptop[1] = ptmp[1];
			}

			//Make sure this line segment is within our range.
//printf( "PT: %d %d %d\n", y, ptop[1], pbot[1] );
			if( ptop[1] <= y && pbot[1] >= y )
			{
				int diffy = pbot[1] - ptop[1];
				uint32_t placey = (uint32_t)(y - ptop[1])<<16;  //Scale by 16 so we can do integer math.
				int diffx = pbot[0] - ptop[0];
				int isectx;

				if( diffy == 0 )
				{
					if( pbot[0] < ptop[0] )
					{
						if( startfillx > pbot[0] ) startfillx = pbot[0];
						if( endfillx < ptop[0] ) endfillx = ptop[0];
					}
					else
					{
						if( startfillx > ptop[0] ) startfillx = ptop[0];
						if( endfillx < pbot[0] ) endfillx = pbot[0];
					}
				}
				else
				{
					//Inner part is scaled by 65536, outer part must be scaled back.
					isectx = (( (placey / diffy) * diffx + 32768 )>>16) + ptop[0];
					if( isectx < startfillx ) startfillx = isectx;
					if( isectx > endfillx ) endfillx = isectx;
				}
//printf( "R: %d %d %d\n", pbot[0], ptop[0], isectx );
			}
		}

//printf( "%d %d %d\n", y, startfillx, endfillx );

		if( endfillx >= bufferx ) endfillx = bufferx - 1;
		if( endfillx >= bufferx ) endfillx = buffery - 1;
		if( startfillx < 0 ) startfillx = 0;
		if( startfillx < 0 ) startfillx = 0;

		unsigned int * bufferstart = &buffer[y * bufferx + startfillx];
		for( x = startfillx; x <= endfillx; x++ )
		{
			(*bufferstart++) = color;
		}
	}
}

void CNOVRCanvasSwapBuffers( cnovr_canvas * c )
{
	CNOVRTextureLoadDataAsync( c->model->pTextures[0], c->w, c->h, 4, 0, c->data );
}

void CNOVRCanvasClearFrame( cnovr_canvas * c )
{
	uint32_t * mark = c->data;
	int count = c->w * c->h;
	uint32_t * end = mark + count;
	uint32_t bgcolor = c->bgcolor;
	while( mark != end ) *(mark++) = bgcolor;
}

