// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#ifndef _CNOVRPARTS_H
#define _CNOVRPARTS_H

#include <GL/gl.h>
#include <os_generic.h>
#include <cnovrmath.h>
#include <stdint.h>
#include <cnovrfocus.h>

typedef struct cnovr_collide_results_t
{
	float t;
	int whichmesh;
	int whichvert;
	float collidepos[3];
	float collidevs[4][4];
} cnovr_collide_results;


struct cnovr_header_t;

typedef struct cnovr_base_t
{
	struct cnovr_header_t * header;
	void * tccctx; //If there was a TCC context involved in the creation of this object (this is used to direct printf)
} cnovr_base;

typedef void (*cnovrfn)( void * ths );

typedef struct cnovr_header_t
{
	cnovrfn Delete;
	cnovrfn Render;
	uint8_t Type;
} cnovr_header;

#define CNOVRRender( x ) x->base.header->Render( x )
#define CNOVRDelete( x )  CNOVRDeleteBase( &(x->base) )
void CNOVRDeleteBase( cnovr_base * b );

#define TYPE_RFBUFFER 1
#define TYPE_SHADER   2
#define TYPE_TEXTURE  3
#define TYPE_MODEL    4
#define TYPE_NODE     5

//////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_rf_buffer_t
{
	cnovr_base base;
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
	cnovr_base base;
	GLuint nShaderID;
	char * shaderfilebase;

	//Temporary list of all watched filenames.
	//struct watchlist
	//{	
	//	struct watchlist * next;
	//	char   watchfile[1];
	//} * tempwl;
} cnovr_shader;

//Call 'render' submethod to activate, and 'prerender' checks to see if anything is tainted.
cnovr_shader * CNOVRShaderCreate( const char * shaderfilebase );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_texture_t
{
	cnovr_base base;
	GLuint nTextureId;
	GLint  nInternalFormat;
	GLenum nFormat;
	GLenum nType;

	uint8_t * data;
	int width;
	int height;
	int channels;
	char * texfile;
	uint8_t bTaintData;
	uint8_t bLoading;
	uint8_t bFileChangeFlag;

	og_mutex_t mutProtect;
} cnovr_texture;


//Defaults to a 1x1 px texture.
cnovr_texture * CNOVRTextureCreate( int initw, int inith, int initchan ); //Set to all 0 to have the load control these details.
int CNOVRTextureLoadFileAsync( cnovr_texture * tex, const char * texfile );
int CNOVRTextureLoadDataAsync( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data ); //Data must be on heap.
int CNOVRTextureLoadDataNow( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data, int data_permanant ); //Must only call from the render/prerender thread.

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
	uint8_t bIsUploaded;
} cnovr_vbo;


//WARNING: Creating or deleting VBOs should not be done by user applcations unless you know what you are doing.  All creation or deletion should be done by the model class.
cnovr_vbo * CNOVRCreateVBO( int iStride, int bDynamic, int iInitialSize, int iAttribNo );
void CNOVRVBOTackv( cnovr_vbo * g, int nverts, float * v );
void CNOVRVBOTack( cnovr_vbo * g,  int nverts, ... ); //Warning: Performance: contents are auto-promoted to doubles and promptly demoted back to floats.
void CNOVRVBOTaint( cnovr_vbo * g );
void CNOVRVBOSetStride( cnovr_vbo * g, int stride );
void CNOVRVBODelete( cnovr_vbo * g );

///////////////////////////////////////////////////////////////////////////////

typedef struct cnovr_model_t
{
	cnovr_base base;

	GLuint nIBO;
	GLuint * pIndices;
	uint32_t iIndexCount;
	GLuint nRenderType;

	//For delineating the index marks per mesh.
	int * iMeshMarks;
	char ** sMeshMarks;
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
	char * geofile;

	cnovr_pose * pose;

	//Focus data TODO: We may be putting this into its own structure to minimize impact on the overall size fo cnovr_model.
	cnovr_pose ** focusgrab; //array, [INPUTDEVS] //If set, currently dragging.
	cnovrfocus_capture * focusevent; //Return collide events with this.
	cnovr_pose * twohandgrab_first; //Points to pose inside focus controller
	float twohandgrab_initialdist;
	cnovr_point3d twohandgrab_space_delta;
	cnovr_quat twohandgrab_q;
	float initial_grab_z[CNOVRINPUTDEVS];

	uint8_t bIsLoading;
	uint8_t bIsUploaded;
	int iOpaque;
} cnovr_model;

//XXX TODO: Reorganize this.

cnovr_model * CNOVRModelCreate( int initial_indices, int num_vbos, int rendertype );


void CNOVRModelSetNumVBOs( cnovr_model * m, int vbos );
void CNOVRModelSetNumVBOsWithStrides( cnovr_model * m, int vbos, ... );

void CNOVRModelTaintIndices( cnovr_model * vm );

void CNOVRModelLoadFromFileAsync( cnovr_model * m, const char * filename ); //Append ".rendermodel" to be an OpenVR rendermodel.
void CNOVRModelTackIndex( cnovr_model * m, int nindices, ...);
void CNOVRModelTackIndexv( cnovr_model * m, int nindices, uint32_t * indices );
void CNOVRModelSetNumIndices( cnovr_model * m, uint32_t indices );
void CNOVRModelResetMarks( cnovr_model * m );

void CNOVRModelAppendCube( cnovr_model * m, cnovr_point3d size, cnovr_pose * poseofs_optional, cnovr_point4d * extradata );
void CNOVRModelAppendMesh( cnovr_model * m, int rows, int cols, int flipv, cnovr_point3d size, cnovr_pose * poseofs_optional, cnovr_point4d * extradata );

//If before first index, names first section.
void CNOVRDelinateGeometry( cnovr_model * m, const char * newGeoName );

int  CNOVRModelCollide( cnovr_model * m, const cnovr_point3d start, const cnovr_vec3d direction, cnovr_collide_results * r );
void CNOVRModelApplyTextureFromFileAsync( cnovr_model * m, const char * sTextureFile );

void CNOVRModelRenderWithPose( cnovr_model * m, cnovr_pose * pose );

// Focus Stuff

void CNOVRModelSetInteractable( cnovr_model * m, cnovrfocus_capture * focusevent );
void CNOVRModelHandleFocusEvent( cnovr_model * m, cnovrfocus_properties * prop, int event, int buttoninfo );

//XXX TODO NOTE: We can set the models up to be "stamped" down with different uniform properties.  

///////////////////////////////////////////////////////////////////////////////
///////////////// Probably should not use nodes, I think they will go away.
typedef struct cnovr_simple_node_t
{
	cnovr_base base;
	cnovr_base ** objects;	//Actually typecasted from whatever the original type is.
	cnovr_pose     pose;
} cnovr_simple_node; 

cnovr_simple_node * CNOVRNodeCreateSimple( int reserved_size ); //reserved size must be at least 1.
void CNOVRNodeAddObject( cnovr_simple_node * node, void * obj );    //O(1)
void CNOVRNodeRemoveObject( cnovr_simple_node * node, void * obj ); //O(n); does not free() or "Delete"

///////////////////////////////////////////////////////////////////////////////


#endif

