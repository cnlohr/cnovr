#ifndef _CNOVRPARTS_H
#define _CNOVRPARTS_H

#include <GL/gl.h>
#include <os_generic.h>

typedef int (*cnovrfn)( void * ths );

struct cnovr_header_t;
typedef struct cnovr_header_t
{
	cnovrfn Delete;
	cnovrfn Update;
	cnovrfn Prerender;
	cnovrfn Render;
} cnovr_header;

#define CNOVRDelete( x ) { if(x) (x)->header.Delete( x ); }


//////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_rf_buffer_t
{
	cnovr_header header;
	GLuint nRenderFramebufferId;
	GLuint nResolveFramebufferId;
	GLuint nResolveTextureId;
	GLuint nDepthBufferId;
	GLuint nRenderTextureId;
} cnovr_rf_buffer;

cnovr_rf_buffer * CNOVRRFBufferCreate( int w, int h, int multisample );
void CNOVRFBufferActivate( cnovr_rf_buffer * b );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_texture_t
{
	cnovr_header header;
	GLuint nTextureId;
	uint8_t * data;
	int width;
	int height;
	int channels;
	const char * texfile;
} cnovr_texture;

cnovr_texture CNOVRTextureCreate( int initw, int inith, int initchan ); //Set to all 0 to have the load control these details.
int CNOVRTextureLoadFile( cnovr_texture * tex, const char * texfile, int lod );
int CNOVRTextureLoadData( cnovr_texture * tex, int w, int h, int chan, void * data, int lod );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_geometry_t
{
	GLuint nIBO;
	GLuint nVBO;

	uint32_t iVertexCount;
	uint32_t iIndexCount;

	int * strides;
	GLfloat * vertices; 
	uint32_t * indices;

	GLuint nRenderType;

	uint8_t bTaint;
	uint8_t nAllowCollisionMask;
	uint8_t iStrides;
	uint8_t bReserved;
} cnovr_geometry;


//XXX TODO: Consider wanting to have static data vs dynamic data as potentially separate arrays unlike how we're holding them here, interleved.
cnovr_geometry * CreateGeo();
void TackGeoVerts( int nverts, ... );
void TackGeoIndices( int ninds, ... );
void TackGeoVertsv( int nverts, float * v );
void TackGeoIndicesv( int ninds, uint32_t * i );



///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_model_t
{
	cnovr_header header;

	cnovr_geometry * pGeos;
	cnovr_texture * textures;
	int iGeos;
	int iTextures;
	og_mutex_t model_mutex;
	const char * geofile;
} cnovr_model;

///////////////////////////////////////////////////////////////////////////////


