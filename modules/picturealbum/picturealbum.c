#include <stdio.h>
#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

#define CNRBTREE_IMPLEMENTATION
#include "cntools/cnrbtree/cnrbtree.h"

int shutting_down;
cnovr_shader * shader;
const char * albumpath;

#define ALBUM_W 7
#define ALBUM_H 7
#define MAX_PICTURES (ALBUM_H*ALBUM_W)

cnovr_model * picture_m[MAX_PICTURES];
cnovrfocus_capture focuscapture[MAX_PICTURES];
cnovr_model * palette_m;
cnovrfocus_capture palettecapture;
int widths[MAX_PICTURES];
int heights[MAX_PICTURES];
char picturestorename[160];

//Bitfield of what outputs NOT to draw on.
int rendermask = 0;
int flag_nearest = 0;

struct staticstore
{
	int initialized;
	cnovr_pose  palettepose;
	cnovr_pose modelpose[MAX_PICTURES];
	float pinned[MAX_PICTURES];
	char filename[MAX_PICTURES][CNOVR_MAX_PATH];
	char filefound[MAX_PICTURES];
} * store;

int PictureFocusEvent( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	cnovr_model * m = (cnovr_model*)cap->opaque;
	int id = m->iOpaque;

	if( event == CNOVRF_LOSTFOCUS )
	{
		printf( "Saving picture album:\"%s\"\n", picturestorename );
		CNOVRNamedPtrSave( picturestorename );
	}

	if( id != -1 )
	{
		switch( event )
		{
			case CNOVRF_DOWNNOFOCUS:
				if( buttoninfo == CTRLA_TRIGGER ) { store->pinned[id] = .999; return 0; } //Catpured event (Return), So, we return the cell to it's place on the palette.
				break;
			case CNOVRF_ACQUIREDFOCUS:
				if( store->pinned[id] < .9 ) store->modelpose[id].Scale *= 2.0;
				store->pinned[id] = 1;
				break;
		}
	}
	else
	{
		if( event == CNOVRF_DOWNNOFOCUS && buttoninfo == CTRLA_TRIGGER )
		{
			int i;
			for( i = 0; i < MAX_PICTURES;i++ )
				store->pinned[i] = .999;
			return 0;
		} //Catpured event (Return), So, we return the cell to it's place on the palette.
	}
	CNOVRGeneralHandleFocusEvent( m->focuscontrol, m->pose, prop, event, buttoninfo, CTRLA_PINCHBTN );
	return 0;
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	int i;
	double truedt = now - start;
	for (i = 0; i < MAX_PICTURES; i++ )
	{
		cnovr_pose * pose = &store->modelpose[i];
		if( !store->filefound[i] )
		{
			pose->Scale = 0;
			continue;
		}
		//Update aspect ratio
		if( picture_m[i]->pTextures && picture_m[i]->pTextures[0] )
		{
			cnovr_texture * t = picture_m[i]->pTextures[0];
			if( t->bTaintData == 0 && t->bLoading == 0 &&
				t->width && t->height && t->width != widths[i] && t->height != heights[i] )
			{
				cnovr_vbo * geo = picture_m[i]->pGeos[0];
				cnovr_vbo * geoextra = picture_m[i]->pGeos[3];
				int side, x, y;
				int rows = 1;
				int cols = 1;
				float aspect = ((float)t->height + 1.0) / t->width;
				widths[i] = t->width;
				heights[i] = t->height;
				for( side = 0; side < 2; side++ )
				for( y = 0; y <= rows; y++ )
				for( x = 0; x <= cols; x++ )
				{
					float * stage = &geo->pVertices[((x + y*2) * 3)+side*12];
					stage[0] = ((side?(x/(float)rows):(1-x/(float)rows))) / ((aspect>1)?aspect:1);
					stage[1] = ((aspect<1)?aspect:1) * y/(float)cols;
					stage[0] = (stage[0] - 0.5)*2.0;
					stage[1] = (stage[1] - 0.5)*2.0;
					stage[2] = 0;

					stage = &geoextra->pVertices[((x + y*2) * 4)+side*16];
					CNOVRVBOTaint( geo );
				}
				glBindTexture( GL_TEXTURE_2D, t->nTextureId );
				if( flag_nearest )
				{
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				}
				else
				{
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
					glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
				}
				glBindTexture( GL_TEXTURE_2D, 0 );

			}
		}

		float * zap = &store->pinned[i];

		if( *zap >= 1.0 ) { continue; } //spinner_n[i]->pose.Scale = .4;
		if( *zap > 0 ) *zap -= .001;
		if( *zap < 0.5 ) *zap = 0;
		float z = *zap;

		//Drag the individual pictures
		//palettepose is where we're dragging to.

		cnovr_point3d targetcenter;
		{
			cnovr_point3d localcenter = { (i % ALBUM_W)*2 - (ALBUM_W-1), -(i/ALBUM_W)*2 + (ALBUM_H-1), .1 };
			scale3d( localcenter, localcenter, 1.0/(ALBUM_W+1) );
			apply_pose_to_point( targetcenter, &store->palettepose, localcenter );
		}
		pose->Pos[0] = cnovr_lerp( targetcenter[0], pose->Pos[0], z );
		pose->Pos[1] = cnovr_lerp( targetcenter[1], pose->Pos[1], z );
		pose->Pos[2] = cnovr_lerp( targetcenter[2], pose->Pos[2], z );
		pose->Scale = cnovr_lerp( pose->Scale, store->palettepose.Scale/(ALBUM_W-1)*0.7, 1.-z );
		quatslerp( pose->Rot, store->palettepose.Rot, pose->Rot, z );
	}

	return;
}


void RenderFunction( void * tag, void * opaquev )
{
	if( (1<<cnovrstate->eyeTarget) & rendermask ) return;

	int i;
	CNOVRRender( shader );
	CNOVRRender( palette_m );
	//glDisable(GL_CULL_FACE);
	for( i = 0; i < MAX_PICTURES; i++ )
	{
		if( store->filefound[i] )
		{
			CNOVRRender( picture_m[i] );
		}
	}
}

static cnovr_model * make_genobj( int i, float gscalex, float gscaley )
{
	cnovr_model * ret = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point4d extradata = { i, 0, 0, 0 };
	CNOVRModelAppendMesh( ret, 1, 1, 1, (cnovr_point3d){  gscalex, gscaley, 0 }, 0, &extradata );
	CNOVRModelAppendMesh( ret, 1, 1, 1, (cnovr_point3d){ -gscalex, gscaley, 0 }, 0, &extradata );
	return ret;
}

static void picturealbum_scene_setup( void * tag, void * opaquev )
{
	printf( "+++ Picture Album setup\n" );
	int i;
	shader = CNOVRShaderCreate( "picturealbum/picturealbum" );

	//////// CREATE MODELS ////////
	palette_m = make_genobj( -1, 1, 1 );
	palette_m->iOpaque = -1;
	palette_m->pose = &store->palettepose;
	CNOVRModelApplyTextureFromFileAsync( palette_m, "picturealbum/container.png" );

	for( i = 0; i < MAX_PICTURES; i++ )
	{
		picture_m[i] = make_genobj( i, 1, 1 );
		picture_m[i]->iOpaque = i;
		picture_m[i]->pose = store->modelpose + i;
	}

	//////// INITIALIZE STORE ////////
	printf( "Initializing Store\n" );
//	store->initialized = 0;
	if( !store->initialized )
	{
		pose_make_identity( &store->palettepose );
		for( i = 0; i < MAX_PICTURES; i++ )
		{
			pose_make_identity( store->modelpose + i );
			store->pinned[i] = 0;
			memset( store->filename[i], 0, sizeof( store->filename[i] ) );
		}
	}
	store->initialized = 1;
	printf( "Populating filenames\n" );

	//////// POPULATE FILENAMES AND RESET SCALES ////////
	for( i = 0; i < MAX_PICTURES; i++ )
	{
		widths[i] = 1;
		heights[i] = 1;
		store->filefound[i] = 0;
	}


	{
		int filecount = 0;
		printf( "Listing files in folder %s\n", albumpath );
		char ** filelist = CNOVRFolderListing( albumpath, &filecount );
		printf( "OK\n" );
		cnstrset * sortset = cnstrset_create();
		for( i = 0; i < filecount; i++ )
		{
			cnstrset_insert( sortset, filelist[i] );
		}
		free( filelist );

		printf( "Populating %d Images\n", filecount );
		char * file;
		cnstrset_foreach( sortset, file )
		{
			int fnamelen = strlen( file );
			if( strstr( file, ".png" ) == 0 && 
				strstr( file, ".PNG" ) == 0 && 
				strstr( file, ".jpg" ) == 0 && 
				strstr( file, ".JPG" ) == 0 ) continue;

			char fncheck[CNOVR_MAX_PATH];
			snprintf( fncheck, CNOVR_MAX_PATH-1 ,"%s/%s", albumpath, file );

			int k;
			for( k = 0; k < MAX_PICTURES; k++ )
			{
				if( strcmp( store->filename[k], fncheck ) == 0 )
				{
					//found it!
					break;
				}
				if( !store->filename[k][0] ) break;
			}

			if( k == MAX_PICTURES ) break;
			if( store->filename[k][0] == 0 )
			{
				//Empty - populate
				strcpy( store->filename[k], fncheck );
				store->filefound[k] = 1;
			}
			else
			{
				//File was found here, we have it in our list, no need to do anything else.
				store->filefound[k] = 1;
			}
		}
		cnstrset_destroy( sortset );
	}

	//Clear out any files which weren't found in the list.
	for( i = 0; i < MAX_PICTURES; i++ )
	{
		if( store->filefound[i] == 0 ) store->filename[i][0] = 0;
	}

	//////// FOCUS SYSTEM ////////

	cnovrfocus_capture * fb;
	for( i = 0; i < MAX_PICTURES;i++ )
	{
		fb = &focuscapture[i];
		fb->tag = 0;
		fb->opaque = picture_m[i];
		fb->cb = PictureFocusEvent;
		CNOVRModelSetInteractable( picture_m[i], fb );
	}
	fb = &palettecapture;
	fb->tag = 0;
	fb->opaque = palette_m;
	fb->cb = PictureFocusEvent;
	CNOVRModelSetInteractable( palette_m, &palettecapture );

	//////// LOAD THE IMAGES ////////

	printf( "Loading Images\n" );
	for( i = 0; i < MAX_PICTURES; i++ )
	{
		char * fname = store->filename[i];
		if( fname[0] == 0 ) continue;
		CNOVRModelApplyTextureFromFileAsync( picture_m[i], fname );
		cnovr_texture * t;
		if( picture_m[i]->pTextures && (t=picture_m[i]->pTextures[0]) )
		{
			t->bCalculateMipMaps = 1;
		}
	}

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );
	//CNOVRListAdd( cnovrLCollide, 0, CollideFunction );
	printf( "+++ Picture Album scene setup complete\n" );
}


void start( const char * identifier )
{
	int len = sprintf( picturestorename, "picturealbumstore_%s", identifier );
	int kx=0;
	for( kx = 0; kx < len; kx++ )
	{
		char cv = picturestorename[kx];
		if( cv == '/' || cv == '\\' || cv == '.' ) picturestorename[kx] = '_';
	}
	store = CNOVRNamedPtrData( picturestorename, 0, sizeof( *store ) + 4096 );
	printf( "=== Initializing Picture Album %p\n", store );

	int elementout = 0;
	char ** identifierstrings = CNOVRSplitStrings( identifier, ",", "", 0, &elementout ); //You can just free(...) the return. it's safe.
	printf( "Identifier with elements: %d %s\n", elementout, identifier );
	if( elementout < 1 )
	{
		printf( "ERROR: Need an identifier for the picture album\n" );
		return;
	}
	for( kx = 1; kx < elementout; kx++ )
	{
		char * str = identifierstrings[kx];
		if( strstr( str, "rmask" ) )
		{
			rendermask = atoi( str + 5 );
		}
		else if( strstr( str, "nearest" ) )
		{
			flag_nearest = 1;
		}
	}
	albumpath = identifierstrings[0];
	CNOVRJobTack( cnovrQPrerender, picturealbum_scene_setup, 0, 0, 0 );
	printf( "=== Picture Album start %s(%p)\n", identifier, identifier );
}

void stop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== End Picuture Album\n" );
}


