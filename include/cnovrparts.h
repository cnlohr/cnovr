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
	uint8_t Type;
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
	uint8_t bLoading;
} cnovr_texture;

//Defaults to a 1x1 px texture.
cnovr_texture CNOVRTextureCreate( int initw, int inith, int initchan ); //Set to all 0 to have the load control these details.
int CNOVRTextureLoadFileAsync( cnovr_texture * tex, const char * texfile, int lod );
int CNOVRTextureLoadData( cnovr_texture * tex, int w, int h, int chan, void * data, int lod );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_vbo_t
{
	GLuint		nVBO;
	uint32_t	iVertexCount;
	int * 		pStrides;
	GLfloat *	pVertices; 

	uint8_t 	bTaint;
	uint8_t 	nAllowCollisionMask;
	uint8_t 	iStrides;
	uint8_t 	bReserved;
} cnovr_vbo;


//XXX TODO: Consider wanting to have static data vs dynamic data as potentially separate arrays unlike how we're holding them here, interleved.
cnovr_vbo * CNOVRCreateVBO();
void CNOVRVBOTackv( cnovr_vbo * g, int stride, int nverts, float * v );
void CNOVRVBOTack( cnovr_vbo * g,  int stride, int nverts, ... ); //Warning: Performance: contents are auto-promoted to doubles and promptly demoted back to floats.
void CNOVRVBOCheck( cnovr_vbo * g, int force );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_collide_results_t
{
	float t;
	int whichmesh;
	int whichtriangle;
	float collidecoords[4][4];
} cnovr_collide_results;

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_model_t
{
	cnovr_header header;

	GLuint nIBO;
	uint32_t * indices;
	uint32_t iIndexCount;
	GLuint nRenderType;

	//For delineating the index marks per mesh.
	int * iMeshMarks;
	int nMeshes;

	cnovr_vbo * pGeos;
	cnovr_texture * textures;
	int iGeos;
	int iTextures;
	og_mutex_t model_mutex;
	const char * geofile;

	uint8_t bTaintIndices;
	uint8_t bIsLoading;
} cnovr_model;

cnovr_model * CNOVRModelCreate();
void CNOVRModelLoadFromFileAsync( cnovr_model * m, const char * filename );
cnovr_geometry * CNOVRModelTackVBO( cnovr_model * m );
void CNOVRModelTack( cnovr_model * m, int nindices, ...);
void CNOVRModelTackv( cnovr_model * m, int nindices, uint32_t * indices );

void CNOVRModelMakeCube( cnovr_model * m, float sx, float sy, float sz );
void CNOVRModelMakeMesh( cnovr_model * m, int rows, int cols, float sx, float sy );
int  CNOVRModelCollide( cnovr_model * m, const cnovr_point3d start, const cnovr_vec3d direction, cnovr_collide_results * r );

void CNOVRModelLoadRenderModelAsync( cnovr_model * m, const char * pchRenderModelName );
void CNOVRModelApplyTextureFromFileAsync( cnovr_model * m, const char * sTextureFile );

void CNOVRModelRenderWithPose( cnovr_model * m, cnovr_pose * pose );



///////////////////////////////////////////////////////////////////////////////

#endif

