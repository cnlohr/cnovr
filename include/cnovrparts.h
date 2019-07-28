#ifndef _CNOVRPARTS_H
#define _CNOVRPARTS_H

#include <GL/gl.h>

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

//////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_texture_t
{
	GLuint nTexture;
	
} cnovr_texture;

//////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_model_t
{
} cnovr_model;

#endif

