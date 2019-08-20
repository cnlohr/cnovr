// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#ifndef _CNOVRPARTS_H
#define _CNOVRPARTS_H

#include <GL/gl.h>
#include <os_generic.h>
#include <cnovrmath.h>

//XXX TODO: Check for updates to things.

typedef int (*cnovrfn)( void * ths );

struct cnovr_header_t;
typedef struct cnovr_header_t
{
	cnovrfn Delete;
	cnovrfn Update;
	cnovrfn Prerender;
	cnovrfn Render;
	uint8_t Type;
} cnovr_header;

#define CNOVRDelete( x ) { if(x) (x)->header.Delete( x ); }

#define TYPE_RFBUFFER 1
#define TYPE_SHADER   2
#define TYPE_TEXTURE  3
#define TYPE_GEOMETRY 4

//////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_rf_buffer_t
{
	cnovr_header header;
	GLuint nRenderFramebufferId;
	GLuint nResolveFramebufferId;
	GLuint nResolveTextureId;
	GLuint nDepthBufferId;
	GLuint nRenderTextureId;

	int width, height;
	int origw, origh; //For holding while activating buffer.
} cnovr_rf_buffer;

cnovr_rf_buffer * CNOVRRFBufferCreate( int w, int h, int multisample );
void CNOVRFBufferActivate( cnovr_rf_buffer * b );
void CNOVRFBufferDeactivate( cnovr_rf_buffer * b );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_shader_t
{
	cnovr_header header;
	GLuint nShaderID;
	const char * shaderfilebase;
	uint8_t bChangeFlag; //geo, frag, vert
} cnovr_shader;

//Call 'render' submethod to activate, and 'prerender' checks to see if anything is tainted.
cnovr_shader * CNOVRShaderCreate( const char * shaderfilebase );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_texture_t
{
	cnovr_header header;
	GLuint nTextureId;
	GLint  nInternalFormat;
	GLenum nFormat;
	GLenum nType;

	uint8_t * data;
	int width;
	int height;
	int channels;
	const char * texfile;
	uint8_t bTaintData;
	uint8_t bLoading;
	uint8_t bFileChangeFlag;

	og_mutex_t mutProtect;
} cnovr_texture;


//Defaults to a 1x1 px texture.
cnovr_texture CNOVRTextureCreate( int initw, int inith, int initchan ); //Set to all 0 to have the load control these details.
int CNOVRTextureLoadFileAsync( cnovr_texture * tex, const char * texfile );
int CNOVRTextureLoadDataAsync( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data ); //data must have been alloc'd on the heap.

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_vbo_t
{
	GLfloat *   pVertices;
	GLuint		nVBO;
	uint32_t	iVertexCount;
	int  		iStride;

	uint8_t 	bTainting;
	uint8_t		bDynamic;

	og_mutex_t  mutData;
} cnovr_vbo;


//WARNING: Creating or deleting VBOs should not be done by user applcations unless you know what you are doing.  All creation or deletion should be done by the model class.
cnovr_vbo * CNOVRCreateVBO( int iStride, int bDynamic, int iInitialSize, int iAttribNo );
void CNOVRVBOTackv( cnovr_vbo * g, int nverts, float * v );
void CNOVRVBOTack( cnovr_vbo * g,  int nverts, ... ); //Warning: Performance: contents are auto-promoted to doubles and promptly demoted back to floats.
void CNOVRVBOTaint( cnovr_vbo * g );
void CNOVRVBOSetStride( cnovr_vbo * g, int stride );
void CNOVRVBODelete( cnovr_vbo * g );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_collide_results_t
{
	float t;
	int whichmesh;
	int whichvert;
	float collidepos[3];
	float collidevs[4][4];
} cnovr_collide_results;

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_model_t
{
	cnovr_header header;

	GLuint nIBO;
	GLuint * pIndices;
	uint32_t iIndexCount;
	GLuint nRenderType;

	//For delineating the index marks per mesh.
	int * iMeshMarks;
	int nMeshes;

	int iLastVertMark;

	//Canonically, it would go:
	//  pGeos[0] = Vertex Position
	//	pGeos[1] = Vertex Texture Coord (or color)
	//	pGeos[2] = Vertex Normal
	cnovr_vbo ** pGeos;
	cnovr_texture ** pTextures;

	int iGeos;
	int iTextures;
	og_mutex_t model_mutex;
	const char * geofile;

	uint8_t bTaintIndices;
	uint8_t bIsLoading;
} cnovr_model;

cnovr_model * CNOVRModelCreate(  int initial_indices, int rendertype );
void CNOVRModelSetNumVBOs( cnovr_model * m, int vbos );
void CNOVRModelLoadFromFileAsync( cnovr_model * m, const char * filename );
void CNOVRModelTack( cnovr_model * m, int nindices, ...);
void CNOVRModelTackv( cnovr_model * m, int nindices, uint32_t * indices );
void CNOVRModelSetNumIndices( cnovr_model * m, uint32_t indices );
void CNOVRModelResetMarks( cnovr_model * m );

void CNOVRModelAppendCube( cnovr_model * m, float sx, float sy, float sz );
void CNOVRModelAppendMesh( cnovr_model * m, int rows, int cols, float sx, float sy );

void CNOVRDelinateGeometry( cnovr_model * m );

int  CNOVRModelCollide( cnovr_model * m, const cnovr_point3d start, const cnovr_vec3d direction, cnovr_collide_results * r );

void CNOVRModelLoadRenderModelAsync( cnovr_model * m, const char * pchRenderModelName );
void CNOVRModelApplyTextureFromFileAsync( cnovr_model * m, const char * sTextureFile );

void CNOVRModelRenderWithPose( cnovr_model * m, cnovr_pose * pose );

//XXX TODO NOTE: We can set the models up to be "stamped" down with different uniform properties.  

///////////////////////////////////////////////////////////////////////////////

#endif

