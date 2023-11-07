#include <cnovrtcc.h>
#include <cnovrparts.h>
#include <cnovrfocus.h>
#include <cnovrcanvas.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <stdlib.h>
#include <string.h>

const char * identifier;
double StartTime;
int shutting_down;

cnovr_shader * Pass1;
cnovr_shader * Pass2;
cnovr_shader * Pass3;
cnovr_shader * DebugPass;
cnovr_shader * basic;
cnovr_shader * test;
cnovr_model * square;
cnovr_rf_buffer * Pass1Buffer;
cnovr_rf_buffer * Pass2Buffer;
cnovr_rf_buffer * Pass3Buffer;

#define GL_RGBA32F 0x8814
#define ARRAYSIZE 128
uint8_t GeoData[ARRAYSIZE*ARRAYSIZE*ARRAYSIZE][4];
uint8_t AddTex[ARRAYSIZE*ARRAYSIZE*ARRAYSIZE][4];
uint8_t MovData[ARRAYSIZE*ARRAYSIZE*ARRAYSIZE][4];
uint8_t AdditionalData[ARRAYSIZE*ARRAYSIZE*ARRAYSIZE][4];
float AttribData[128*256][4];

#define NOISESIZE 16
float NoiseData[NOISESIZE*NOISESIZE][4];

GLuint geoDataTextureId;
GLuint addDataTextureId;
GLuint movDataTextureId;
GLuint additionalDataTextureId;
GLuint attribDataTextureId;
GLuint noiseDataTextureId;

struct staticstore
{
	int initialized;
	//cnovr_pose modelpose[MAX_CHAIRS];
	//cnovr_pose targetpose[MAX_CHAIRS];
	//float zapped[MAX_CHAIRS];
} * store;


void RenderFunction( void * tag, void * opaquev )
{
	double EuclidTime = 100.; OGGetAbsoluteTime()-StartTime;

	if( cnovrstate->eyeTarget == 2 ) return;
	
	if( cnovrstate->eyeTarget < 2 )
		CNOVRFBufferDeactivate( cnovrstate->stereotargets[cnovrstate->eyeTarget] );

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	if( !Pass1Buffer || Pass1Buffer->width != cnovrstate->iEyeRenderWidth ||
		Pass1Buffer->height != cnovrstate->iEyeRenderHeight )
	{
		if( Pass1Buffer )
		{
			CNOVRDelete( Pass1Buffer );
		}
		Pass1Buffer = CNOVRRFBufferCreate( cnovrstate->iEyeRenderWidth, cnovrstate->iEyeRenderHeight, RFBUFFER_FLAGS_RGBA32F, 4 );
	}

	if( !Pass2Buffer || Pass2Buffer->width != cnovrstate->iEyeRenderWidth ||
		Pass2Buffer->height != cnovrstate->iEyeRenderHeight )
	{
		if( Pass2Buffer )
		{
			CNOVRDelete( Pass2Buffer );
		}
		Pass2Buffer = CNOVRRFBufferCreate( cnovrstate->iEyeRenderWidth, cnovrstate->iEyeRenderHeight, RFBUFFER_FLAGS_RGBA32F, 4 );
	}

	if( !Pass3Buffer || Pass3Buffer->width != cnovrstate->iEyeRenderWidth ||
		Pass3Buffer->height != cnovrstate->iEyeRenderHeight )
	{
		if( Pass3Buffer )
		{
			CNOVRDelete( Pass3Buffer );
		}
		Pass3Buffer = CNOVRRFBufferCreate( cnovrstate->iEyeRenderWidth, cnovrstate->iEyeRenderHeight, RFBUFFER_FLAGS_RGBA32F, 4 );
	}

	cnovrstate->iRTWidth = cnovrstate->iEyeRenderWidth;
	cnovrstate->iRTHeight = cnovrstate->iEyeRenderHeight;

	GLfloat matrix[16];
	memset( matrix, 0, sizeof( matrix ) );
	matrix[0] = 1;
	matrix[5] = 1;
	matrix[10] = 1;
	matrix[15] = 1;
	int uniform;
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( UNIFORMSLOT_MODEL ) ) != INVALIDUNIFORM )
		glUniformMatrix4fv( uniform, 1, 1, matrix );


	CNOVRFBufferActivate( Pass1Buffer );

	CNOVRRender( Pass1 );
	//umModel = 4

	glEnable( GL_TEXTURE_3D );
	glEnable( GL_TEXTURE_2D );
	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_3D, geoDataTextureId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture( GL_TEXTURE_3D, addDataTextureId);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture( GL_TEXTURE_3D, movDataTextureId);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture( GL_TEXTURE_3D, additionalDataTextureId);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture( GL_TEXTURE_2D, attribDataTextureId);

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 14 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM GeoTex 14
		glUniform1i( uniform, 0 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 15 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AddTex 15
		glUniform1i( uniform, 1 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 16 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM MovTex 16
		glUniform1i( uniform, 2 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 17 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AdditionalInformationMap 17
		glUniform1i( uniform, 3 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 18 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM AttribMap 18
		glUniform1i( uniform, 4 );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 19 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msX 19
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 20 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msY 20
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 21 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msZ 21
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 22 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM time 22
		glUniform1f( uniform, EuclidTime );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 23 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM mixval 23
		glUniform1f( uniform, .9 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 24 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM densitylimit 24
		glUniform1f( uniform, .2 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 25 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM densitymux 25
		glUniform1f( uniform, 1.0 );

	CNOVRRender( square );
	CNOVRFBufferDeactivate( Pass1Buffer );
	CNOVRFBufferBlitResolve( Pass1Buffer );



	CNOVRFBufferActivate( Pass2Buffer );

	CNOVRRender( Pass2 );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 14 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM GeoTex 14
		glUniform1i( uniform, 0 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 15 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AddTex 15
		glUniform1i( uniform, 1 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 16 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM MovTex 16
		glUniform1i( uniform, 2 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 17 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AdditionalInformationMap 17
		glUniform1i( uniform, 3 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 18 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM AttribMap 18
		glUniform1i( uniform, 4 );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 27 ) ) != INVALIDUNIFORM )   //#MAPUNIFORM Pass1A 27
		glUniform1i( uniform, 5 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 28 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM Pass1B 28
		glUniform1i( uniform, 6 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 29 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM NoiseMap 29
		glUniform1i( uniform, 7 );

	glActiveTexture(GL_TEXTURE5);
	glBindTexture( GL_TEXTURE_2D, Pass1Buffer->nResolveTextureId[0] );
    glActiveTexture(GL_TEXTURE6);
    glBindTexture( GL_TEXTURE_2D, Pass1Buffer->nResolveTextureId[1] );
    glActiveTexture(GL_TEXTURE7);
    glBindTexture( GL_TEXTURE_2D, noiseDataTextureId );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 19 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msX 19
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 20 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msY 20
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 21 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msZ 21
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 22 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM time 22
		glUniform1f( uniform, EuclidTime );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 26 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM do_subtrace 26
		glUniform1f( uniform, 1.0 );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 30 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM ScreenX 30
		glUniform1f( uniform, cnovrstate->iEyeRenderWidth );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 31 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM ScreenY 31
		glUniform1f( uniform, cnovrstate->iEyeRenderHeight );

	CNOVRRender( square );
	CNOVRFBufferDeactivate( Pass2Buffer );
	CNOVRFBufferBlitResolve( Pass2Buffer );









	CNOVRFBufferActivate( Pass3Buffer );

	CNOVRRender( Pass3 );



	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_3D, geoDataTextureId);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture( GL_TEXTURE_3D, addDataTextureId);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture( GL_TEXTURE_2D, attribDataTextureId);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture( GL_TEXTURE_2D, Pass1Buffer->nResolveTextureId[0] );
    glActiveTexture(GL_TEXTURE4);
    glBindTexture( GL_TEXTURE_2D, Pass1Buffer->nResolveTextureId[1] );
    glActiveTexture(GL_TEXTURE5);
    glBindTexture( GL_TEXTURE_2D, noiseDataTextureId );
    glActiveTexture(GL_TEXTURE6);
    glBindTexture( GL_TEXTURE_2D, Pass2Buffer->nResolveTextureId[0] );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 23 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM GeoTex 23
		glUniform1i( uniform, 0 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 24 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AddTex 24
		glUniform1i( uniform, 1 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 25 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM AttribMap 25
		glUniform1i( uniform, 2 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 26 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM Pass1A 26
		glUniform1i( uniform, 3 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 27 ) ) != INVALIDUNIFORM )   //#MAPUNIFORM Pass1B 27
		glUniform1i( uniform, 4 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 28 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM NoiseMap 28
		glUniform1i( uniform, 5 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 29 ) ) != INVALIDUNIFORM )  //#MAPUNIFORM Pass2 29
		glUniform1i( uniform, 6 );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 19 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msX 19
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 20 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msY 20
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 21 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM msZ 21
		glUniform1f( uniform, ARRAYSIZE );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 22 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM time 22
		glUniform1f( uniform, EuclidTime );

	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 18 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM do_subtrace 18
		glUniform1f( uniform, 1.0 );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 30 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM ScreenX 30
		glUniform1f( uniform, cnovrstate->iEyeRenderWidth );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 31 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM ScreenY 31
		glUniform1f( uniform, cnovrstate->iEyeRenderHeight );

	CNOVRRender( square );
	CNOVRFBufferDeactivate( Pass3Buffer );
	CNOVRFBufferBlitResolve( Pass3Buffer );

	if( cnovrstate->eyeTarget < 2 )
		CNOVRFBufferActivate( cnovrstate->stereotargets[cnovrstate->eyeTarget] );


	CNOVRRender( DebugPass );
	glEnable( GL_TEXTURE_2D );
	if( ( uniform = CNOVRMAPPEDUNIFORMPOS( 15 ) ) != INVALIDUNIFORM ) //#MAPUNIFORM MarkTex 15
		glUniform1i( uniform, 1 );

	//printf( "%d %d %d\n", Pass1Buffer->nResolveTextureId[0], Pass1Buffer->nResolveTextureId[1], Pass1Buffer->nResolveTextureId[2] );
	glActiveTexture(GL_TEXTURE0);
	glBindTexture( GL_TEXTURE_2D, Pass3Buffer->nResolveTextureId[0] );
	glActiveTexture(GL_TEXTURE1);
	glBindTexture( GL_TEXTURE_2D, Pass3Buffer->nResolveTextureId[1] );
	glActiveTexture(GL_TEXTURE2);
	glBindTexture( GL_TEXTURE_2D, Pass1Buffer->nResolveTextureId[2] );
	CNOVRRender( square );

	glDisable(GL_CULL_FACE);
	
}

void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	static double last;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) { last = start = now; }
}

void SetupTextureData3D( GLuint * tex, uint8_t * data, int x, int y, int z )
{
	glGenTextures( 1, tex );
	glBindTexture( GL_TEXTURE_3D, *tex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, x, y, z, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
    glBindTexture(GL_TEXTURE_3D, 0);
}

static void example_scene_setup( void * tag, void * opaquev )
{
	int i, j;

	glEnable( GL_TEXTURE_2D );
	glEnable( GL_TEXTURE_3D );

	SetupTextureData3D( &geoDataTextureId, &GeoData[0][0], ARRAYSIZE, ARRAYSIZE, ARRAYSIZE  );
	SetupTextureData3D( &addDataTextureId, &AddTex[0][0],  ARRAYSIZE, ARRAYSIZE, ARRAYSIZE );
	SetupTextureData3D( &movDataTextureId, &MovData[0][0], ARRAYSIZE, ARRAYSIZE, ARRAYSIZE );
	SetupTextureData3D( &additionalDataTextureId, &AdditionalData[0][0], ARRAYSIZE, ARRAYSIZE, ARRAYSIZE );

	glGenTextures( 1, &attribDataTextureId );
	glBindTexture( GL_TEXTURE_2D, attribDataTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 128, 256, 0, GL_RGBA, GL_FLOAT, AttribData );
    glBindTexture(GL_TEXTURE_2D, 0);


	glGenTextures( 1, &noiseDataTextureId );
	glBindTexture( GL_TEXTURE_2D, noiseDataTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, NOISESIZE, NOISESIZE, 0, GL_RGBA, GL_FLOAT, NoiseData );
    glBindTexture(GL_TEXTURE_2D, 0);

	UpdateFunction(0,0);
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	CNOVRListAdd( cnovrLRender2, 0, RenderFunction );

	printf( "+++ Example scene setup\n" );
}



void noeuclidteststart( const char * identifier )
{
	store = CNOVRNamedPtrData( "noeuclidtest", 0, sizeof( *store ) + 1024 );
	printf( "=== Initializing %p\n", store );

	if( !store->initialized || 1 )
	{
		store->initialized = 1;
	}

	DebugPass = CNOVRShaderCreateWithPrefix( "DebugPass", "#define SOMETHING_SOMETHING" );
	Pass1 = CNOVRShaderCreateWithPrefix( "Pass1", "#define SOMETHING_SOMETHING" );
	Pass2 = CNOVRShaderCreateWithPrefix( "Pass2", "#define SOMETHING_SOMETHING" );
	Pass3 = CNOVRShaderCreateWithPrefix( "Pass3", "#define SOMETHING_SOMETHING" );
	basic = CNOVRShaderCreateWithPrefix( "basic", "#define SOMETHING_SOMETHING" );
	test = CNOVRShaderCreateWithPrefix( "test", "#define SOMETHING_SOMETHING" );
	printf( "Shaders compiled: %p %p %p\n", Pass1, Pass2, Pass3 );

	square = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point3d size = { 1, 1, 1 };
//	CNOVRModelAppendCube( square, size, 0, 0 );
	CNOVRModelAppendMesh( square, 1, 1, 0, (cnovr_point3d){ 1, 1, 1 }, 0, 0 /*extradata*/ );

	{
		int iLine;
		int iTile;
		for (unsigned iLine = 0; iLine < 256; iLine++) //actually cell type
		{
		    for (unsigned iTile = 0; iTile < 16; iTile++) //actually meta's.
		    {
				memcpy( AttribData[iLine * 128 + iTile * 8 + 0], 
					(float[4]){ 1, 1, 1, 1 }, sizeof( float[4] ) );
				memcpy( AttribData[iLine * 128 + iTile * 8 + 1], 
					(float[4]){ 1, 1, 1, 1 }, sizeof( float[4] ) );
				memcpy( AttribData[iLine * 128 + iTile * 8 + 2], 
					(float[4]){ 0, 0, 0, 0 }, sizeof( float[4] ) );
				memcpy( AttribData[iLine * 128 + iTile * 8 + 3], 
					(float[4]){ 0, 0, 0, 0 }, sizeof( float[4] ) );
				memcpy( AttribData[iLine * 128 + iTile * 8 + 4], 
					(float[4]){ 1, 1, .1, 1 }, sizeof( float[4] ) );
				memcpy( AttribData[iLine * 128 + iTile * 8 + 7], 
					(float[4]){ 1, 1, 0, 1 }, sizeof( float[4] ) );
		    }
		}



		int attrib_file_length;
		char * attrib_file = CNOVRFileToString( "projects/noeuclidtest/tileattributes.txt", &attrib_file_length );

		if( attrib_file )
		{
			int linect;
			char ** lines =  CNOVRSplitStrings( attrib_file, "\n", "\r \t", 1, &linect ); //You can just free(...) the return. it's safe.
			int i;
			for( i = 0; i < linect; i++ )
			{
				char * thisline = lines[i];
				if( thisline[0] == '#' ) continue;
				char blockname[128];
				int readct = 0;
				char ** secs = CNOVRSplitStrings( thisline, " \t", "", 1, &readct );

				if( readct < 4 )
				{
					printf( "Error, insufficient parameters on line %d\n", i );
					continue;
				}
		        int iTileID = -1, iMetaID = -1;
				char * Description = secs[0];
				iTileID = atoi( secs[1] );
				iMetaID = atoi( secs[2] );
				float fDensity = atof( secs[3] );
				int m;

				//Not sure why this is here, but No! Euclid! Did it.

				if( readct > 36 ) readct = 36;
				float tileparams[8][4];

				memcpy( tileparams, AttribData[iTileID * 128 + iMetaID * 8], 
					sizeof( float[8*4] ) );

				memcpy( tileparams[7], 
					(float[4]){ 1, 1, 1, 1 }, sizeof( float[4] ) );

				for( m = 4; m < readct; m++ )
				{
					tileparams[0][(m - 4)] = atof( secs[m] );
				}
				tileparams[7][3] = fDensity;
		        if (iMetaID == -1) {
		            for (iMetaID = 0; iMetaID < 16; iMetaID++) {
						memcpy( AttribData[iTileID * 128 + iMetaID * 8], tileparams, sizeof(float)*8*4 );
					}
				}
				else
				{
					memcpy( AttribData[iTileID * 128 + iMetaID * 8], tileparams, sizeof(float)*8*4 );
				}
				free( secs );
			}
			free( lines );
		}
	}

	{
		int blkid = 10;

		int x, y, z;
		for( z = 0; z < ARRAYSIZE; z++ )
		for( y = 0; y < ARRAYSIZE; y++ )
		for( x = 0; x < ARRAYSIZE; x++ )
		{
			MovData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][0] = 50;
			MovData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][1] = 200;
			MovData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][2] = 100;
			MovData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][3] = 255;

			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][0] = blkid;	//ID
			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][1] = 255;	//METADATA
			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][3] = blkid;	//ID
		}

		for( z = 0; z < ARRAYSIZE; z++ )
		for( x = 0; x < ARRAYSIZE; x++ )
		{
			int y = 127;
			int density = (x+z)&1;
			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][0] = blkid;	//ID
			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][2] = density?103:0;	//Density to draw.
			GeoData[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][3] = blkid;	//"Actual Cell to Draw"
			AddTex[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][0] = 255;	//Density (Does it exist)
			AddTex[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][1] = 0;		//???
			AddTex[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][2] = 0;		//Used for jump maps
			AddTex[x+y*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][3] = 0;		//Used for jump maps
			//MovData is unused until we have jump maps running.  OR NOT! Apparently this is important?


			AddTex[x+(0)*ARRAYSIZE+z*ARRAYSIZE*ARRAYSIZE][0] = 255;	//Density (Does it exist)
		}
	}

	int i;
	for( i = 0; i < NOISESIZE*NOISESIZE*4; i++ )
	{
		((float*)NoiseData[0])[i] = (rand() % 16384) / 16384.0f;
	}

/*	memset( GeoData, 0xff, sizeof( GeoData ) );
	memset( AddTex, 0xff, sizeof( AddTex ) );
	memset( MovData, 0xff, sizeof( MovData ) );
	memset( AdditionalData, 0xff, sizeof( AdditionalData ) );
*/

	StartTime = OGGetAbsoluteTime();
	identifier = strdup(identifier);
	CNOVRJobTack( cnovrQPrerender, example_scene_setup, 0, 0, 0 );
	printf( "=== NoEuclidTest start %s(%p) + %p %p\n", identifier, identifier );
}

void noeuclidteststop( const char * identifier )
{
	shutting_down = 1;
	//OGCancelThread( thdmax );
	printf( "=== NoEuclidTest stop\n" );
}



