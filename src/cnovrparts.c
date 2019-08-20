// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#include <cnovrparts.h>
#include <chew.h>
#include <string.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static void CNOVRRenderFrameBufferDelete( cnovr_rf_buffer * ths )
{
	if( ths->nRenderFramebufferId ) glDeleteFramebuffers( 1, &ths->nRenderFramebufferId );
	if( ths->nResolveFramebufferId ) glDeleteFramebuffers( 1, &ths->nResolveFramebufferId );
	if( ths->nResolveTextureId ) glDeleteTextures( 1, &ths->nResolveTextureId );
	if( ths->nDepthBufferId ) glDeleteRenderbuffers( 1, &ths->nDepthBufferId );
	if( ths->nRenderTextureId ) glDeleteTextures( 1, &ths->nRenderTextureId );

	free( ths );
}

cnovr_rf_buffer * CNOVRRFBufferCreate( int nWidth, int nHeight, int multisample )
{
	cnovr_rf_buffer * ret = malloc( sizeof( cnovr_rf_buffer ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->header.Delete = (cnovrfn)CNOVRRenderFrameBufferDelete;
	ret->header.Type = 1;

	glGenFramebuffers(1, &ret->nRenderFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, ret->nRenderFramebufferId);

	glGenRenderbuffers(1, &ret->nDepthBufferId);
	glBindRenderbuffer(GL_RENDERBUFFER, ret->nDepthBufferId);
	if( multisample )
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, GL_DEPTH_COMPONENT, nWidth, nHeight );
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,	ret->nDepthBufferId );

	glGenTextures(1, &ret->nRenderTextureId );
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ret->nRenderTextureId );
	if( multisample )
		glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, multisample, GL_RGBA8, nWidth, nHeight, 1);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ret->nRenderTextureId, 0);

	glGenFramebuffers(1, &ret->nResolveFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, ret->nResolveFramebufferId);

	glGenTextures(1, &ret->nResolveTextureId );
	glBindTexture(GL_TEXTURE_2D, ret->nResolveTextureId );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ret->nResolveTextureId, 0);

	// check FBO status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		CNOVRDeleteRenderFrameBuffer( ret );
		return 0;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	return ret;
}


void CNOVRFBufferActivate( cnovr_rf_buffer * b )
{
	b->origw = cnovrstate->iRTWidth;
	b->origh = cnovrstate->iRTHeight;
	int w = cnovrstate->iRTWidth = b->width;
	int h = cnovrstate->iRTHeight = b->height;
	glBindFramebuffer( GL_FRAMEBUFFER, b->nResolveFramebufferId );
 	glViewport(0, 0, w, h );
}

void CNOVRFBufferDeactivate( cnovr_rf_buffer * b )
{
 	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	int w = cnovrstate->iRTWidth = b->origw;
	int h = cnovrstate->iRTHeight = b->origh;
 	glViewport(0, 0, w, h );
}

// Just FYI to blit a buffer:
//	glBindFramebuffer(GL_READ_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, leftEyeDesc.m_nResolveFramebufferId );
//	glBlitFramebuffer( 0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, 
//		GL_COLOR_BUFFER_BIT,
// 		GL_LINEAR );
// 	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );	



//////////////////////////////////////////////////////////////////////////////

static void CNOVRShaderDelete( cnovr_shader * ths )
{
	if( ths->glDeleteProgram ) glDeleteProgram( glDeleteProgram );
	FileTimeRemoveTagged( ths );
	free( ths->shaderfilebase );
	free( ths );
}

static GLuint CNOVRShaderCompilePart( GLuint shader_type, const char * shadername, const char * compstr )
{
	GLuint nShader = glCreateShader( shader_type );
	glShaderSource( nShader, 1, &compstr, NULL );
	glCompileShader( nShader );
	GLint vShaderCompiled = GL_FALSE;

	glGetShaderiv( nShader, GL_COMPILE_STATUS, &vShaderCompiled );
	if ( vShaderCompiled != GL_TRUE )
	{
		CNOVRAlert( cnovrstate->pCurrentModel, "Unable to compile shader: %s\n", shadername );
		int retval;
		glGetShaderiv( nShader, GL_INFO_LOG_LENGTH, &retval );
		if ( retval > 1 ) {
			char * log = (char*)malloc( retval );
			glGetShaderInfoLog( nShader, retval, NULL, log );
			CNOVRAlert( cnovrstate->pCurrentModel, "%s\n", log );
			free( log );
		}

		glDeleteShader( nShader );
		return 0;
	}
	return nShader;
}


static void CNOVRShaderPrerender( cnovr_shader * ths )
{
	if( ths->bChangeFlag )
	{
		//Re-load shader

		//Careful: Need to re-try in case a program is still writing.
		if( ths->bChangeFlag++ > 2 )
		{
			ths->bChangeFlag = 0;
			return;
		}
		
		GLuint nGeoShader  = 0;
		GLuint nFragShader = 0;
		GLuint nVertShader = 0;

		const char * filedataGeo = 0;
		const char * filedataFrag = 0;
		const char * filedataVert = 0;
		{
			char stfb[MAX_PATH];
	 
			sprintf( stfb, "%s.geo", ths->shaderfilebase );
			filedataGeo = FileToString( stfb, 0 );
			sprintf( stfb, "%s.frag", ths->shaderfilebase );
			filedataFrag = FileToString( stfb, 0 );
			sprintf( stfb, "%s.vert", ths->shaderfilebase );
			filedataVert = FileToString( stfb, 0 );
		}

		if( !filedataFrag || !filedataVert )
		{
			CNOVRAlert( cnovrstate->pCurrentModel, "Unable to open vert/frag in shader: %s\n", ths->shaderfilebase );
			return;
		}

		if( filedataGeo )
		{
			nGeoShader = CNOVRShaderCompilePart( GL_GEOMETRY_SHADER, stfb, filedataGeo );
		}
		nFragShader = CNOVRShaderCompilePart( GL_FRAGMENT_SHADER, stfb, filedataFrag );
		nVertShader = CNOVRShaderCompilePart( GL_VERTEX_SHADER, stfb, filedataVert );

		bool compfail = false;
		if( filedataGeo )
		{
			if ( !nGeoShader )
				compfail = true;
		}
		if ( !nVertShader || !nFragShader )
		{
			compfail = true;
		}

		GLuint unProgramID = 0;

		if ( !compfail )
		{
			unProgramID = glCreateProgram();
			if ( GeoData.length() )
			{
				glAttachShader( unProgramID, nGeoShader );
			}
			glAttachShader( unProgramID, nFragShader );
			glAttachShader( unProgramID, nVertShader );
			glLinkProgram( unProgramID );

			GLint programSuccess = GL_TRUE;
			glGetProgramiv( unProgramID, GL_LINK_STATUS, &programSuccess );
			if ( programSuccess != GL_TRUE )
			{
				CNOVRAlert( cnovrstate->pCurrentModel, "Shader linking failed: %s\n", ths->shaderfilebase );
				int retval;
				glGetShaderiv( unProgramID, GL_INFO_LOG_LENGTH, &retval );
				if ( retval > 1 ) {
					char * log = (char*)malloc( retval );
					glGetProgramInfoLog( unProgramID, retval, NULL, log );
					CNOVRAlert( cnovrstate->pCurrentModel, "%s\n", log );
					free( log );
				}
				glDeleteProgram( unProgramID );
				compfail = true;
			}
		}
		else
		{
			CNOVRAlert( cnovrstate->pCurrentModel, "Shader compilation failed: %s\n", ths->shaderfilebase );
		}

		if ( unProgramID )
		{
			if ( ths->nShaderID )
			{
				ths->bChangeFlag = 0;
				glDeleteProgram( ths->nShaderID );
			}
			ths->nShaderID = unProgramID;
		}

		if( nGeoShader )  glDeleteShader( nGeoShader );
		if( nFragShader ) glDeleteShader( nFragShader );
		if( nVertShader ) glDeleteShader( nVertShader );
	}
}

static void CNOVRShaderRender( cnovr_shader * ths )
{
	if( ths->markforcheck )
	{
		ths->markforcheck = 0;
	}

	int shdid = ths->nShaderID;
	if( !shdid ) return;
	glUseProgram( shdid );
	glUniform4f( 16, cnovrstate->iRTWidth, cnovrstate->iRTHeight, cnovrstate->fFar, cnovrstate->fNear );
	glUniformMatrix4fv( 17, 1, 0, cnovrstate->mPerspective ); 
	glUniformMatrix4fv( 18, 1, 0, cnovrstate->mView ); 
	glUniformMatrix4fv( 19, 1, 0, cnovrstate->mModel ); 
};

cnovr_shader * CNOVRShaderCreate( const char * shaderfilebase )
{
	cnovr_rf_buffer * ret = malloc( sizeof( cnovr_rf_buffer ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->header.Delete = (cnovrfn)CNOVRShaderDelete;
	ret->header.Render = (cnovrfn)CNOVRShaderRender;
	ret->header.Prerender = (cnovrfn)CNOVRShaderPrerender;
	ret->header.Type = TYPE_SHADER;
	ret->shaderfilebase = strdup( shaderfilebase );

	char stfb[MAX_PATH];
	sprintf( stfb, "%s.geo", shaderfilebase );
	FileTimeAddWatch( stfb, &ret->changeflag, ret );
	sprintf( stfb, "%s.frag", shaderfilebase );
	FileTimeAddWatch( stfb, &ret->changeflag, ret );
	sprintf( stfb, "%s.vert", shaderfilebase );
	FileTimeAddWatch( stfb, &ret->changeflag, ret );

	ret->changeflag = 1;
}

//////////////////////////////////////////////////////////////////////////////

static void CNOVRTextureLoadFileTask( void * opaquev, int opaquei )
{
	cnovr_texture * t = (cnovr_texture*)opaquev;

	OGLockMutex( ths->mutProtect );
	char * localfn = strdup( t->texfile );
	OGUnlockMutex( ths->mutProtect );

	int x, y, chan;
	stbi_uc * data = stbi_load( localfn, &x, &y, &chan, 4 );
	free( localfn );

	if( data )
	{
		CNOVRTextureLoadDataAsync( t, x, y, chan, 0, data );
		t->bLoading = 0;
	}

}

static void CNOVRTextureDelete( cnovr_texture * ths )
{
	OGLockMutex( ths->mutProtect );
	//In case any file changes are being watched.
	FileTimeRemoveTagged( ths );

	//Pre-emptively kill any possible pending queued events.
	CNOVRJobCancel( cnovrQAsync, CNOVRLoadFile, ths, 0 );
	CNOVRJobCancel( cnovrQPrerender, CNOVRTextureUploadCallback, ths, 0 );

	if( ths->nTextureId )
	{
		glDeleteTextures( 1, &ths->nTextureId );
	}
	if( ths->data ) free( ths->data );
	if( ths->texfile ) free( ths->texfile );
	OGDeleteMutex( ths->mutProtect );
	free( ths );
}

static void CNOVRTextureRender( cnovr_texture * ths )
{
	glBindTexture( GL_TEXTURE_2D, ths->nTextureId );
}

static void CNOVRTextureUploadCallback( cnovr_texture * ths, int i )
{
	OGLockMutex( ths->mutProtect );
	ths->bTaintData = 0;
	glBindTexture( GL_TEXTURE_2D, ths->nTextureId );
	glTexImage2D( GL_TEXTURE_2D,
		0,
		ths->nInternalFormat,
		ths->width,
		ths->height,
		0,
		ths->nFormat,
		ths->nType,
		ths->data );
	glBindTexture( GL_TEXTURE_2D, 0 );
	OGUnlockMutex( ths->mutProtect );
}

//Defaults to a 1x1 px texture.
cnovr_texture CNOVRTextureCreate( int initw, int inith, int initchan )
{
	cnovr_texture * ret = malloc( sizeof( cnovr_rf_buffer ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->header.Delete = (cnovrfn)CNOVRTextureDelete;
	ret->header.Render = (cnovrfn)CNOVRTextureRender;
	ret->header.Type = TYPE_TEXTURE;
	ret->texfile = 0;

	ret->mutProtect = OGCreateMutex();

	glGenTextures( 1, &ret->nTextureId );

	ret->width = 1;
	ret->height = 1;
	ret->channels = 4;
	ret->data = malloc( 4 );
	ret->bTaintData = 0;
	ret->bLoading = 0;
	ret->bFileChangeFlag = 0;
	memset( ret->data, 255, 4 );

	glBindTexture( GL_TEXTURE_2D, ret->nTextureId );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, ret->width, ret->height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, ret->data );

	glBindTexture( GL_TEXTURE_2D, 0 );

	return ret;
}

int CNOVRTextureLoadFileAsync( cnovr_texture * tex, const char * texfile )
{
	OGLockMutex( ths->mutProtect );
	if( tex->texfile ) free( tex->Texfile );
	tex->texfile = strdup( texfile );
	tex->bLoading = 1;
	CNOVRJobCancel( cnovrQAsync, CNOVRTextureLoadFileTask, tex, 0 ); //Just in case.
	CNOVRJobTack( cnovrQAsync, CNOVRTextureLoadFileTask, tex, 0 );
	OGUnlockMutex( ths->mutProtect );
}

int CNOVRTextureLoadDataAsync( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data )
{
	OGLockMutex( tex->mutProtect );

	if( tex->data ) free( tex->data );
	tex->data = data;
	tex->width = w;
	tex->height = h;
	tex->channels = chan;

	static const int channelmapF[] = { 0, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
	static const int channelmapI[] = { 0, GL_RF, GL_RG8, GL_RGB8, GL_RGBA8 };
	static const int channelmapB[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };

	if( is_float )
		ths->nInternalFormat = channelmapF[chan];
	else
		ths->nInternalFormat = channelmapI[chan];
	ths->nFormat = channelmapB[chan];
	ths->nType = is_float?GL_FLOAT:GL_UNSIGNED_BYTE;
	ths->bTaintData = 1;
	CNOVRJobTack( cnovrQPrerender, CNOVRTextureUploadCallback, tex, 0 );
	OGUnlockMutex( tex->mutProtect );
}



///////////////////////////////////////////////////////////////////////////////

cnovr_vbo * CNOVRCreateVBO( int iStride, int bDynamic, int iInitialSize, int iAttribNo )
{
	cnovr_vbo * ret = malloc( sizeof( cnovr_vbo ) );
	memset( ret, 0, sizeof( cnovr_vbo ) );
	ret->iVertexCount = iInitialSize;

	if( iInitialSize < 1 ) iInitialSize = 1;
	ret->pVertices = malloc( iInitialSize * sizeof(float) * iStride );
	ret->iStride = iStride;

	glGenBuffers( 1, &ret->nVBO );
	ret->bDynamic = bDynamic;
	ret->mutData = OGCreateMutex();

	glBindBuffer(GL_ARRAY_BUFFER, VertexVBOID);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableVertexAttribArray( iAttribNo );
	glVertexPointer( iStride, GL_FLOAT, 0, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return ret;
}


void CNOVRVBOTackv( cnovr_vbo * g, int nverts, float * v )
{
	CNOVRLockMutex( g->mutData );
	int stride = g->iStride;
	ret->pVertices = realloc( ret->pVertices, (g->iVertexCount+1)*stride*sizeof(float) );
	float * verts = ret->pVertices + (g->iVertexCount*stride);
	int tocopy = nverts;
	if( nverts > stride ) tocopy = stride;
	int i;
	for( i = 0; i < tocopy; i++ )
	{
		verts[i] = v[i];
	}
	for( ; i < stride; i++ )
	{
		verts[i] = 0;
	}
	g->iVertexCount++;
	CNOVRUnlockMutex( g->mutData );
	CNOVRVBOTaint( g );
}

void CNOVRVBOTack( cnovr_vbo * g,  int nverts, ... )
{
	CNOVRLockMutex( g->mutData );
	int stride = g->iStride;
	ret->pVertices = realloc( ret->pVertices, (g->iVertexCount+1)*stride*sizeof(float) );
	float * verts = ret->pVertices;
	va_list argp;
	va_start( argp, nverts );
	int i;
	for( i = 0; i < stride; i++ )
	{
		verts[i] = va_arg(argp, double);
	}
	ret->pVertices++;
	va_end( argp );
	CNOVRUnlockMutex( g->mutData );
	CNOVRVBOTaint( g );
}

static void CNOVRVBOPerformUpload( cnovr_vbo * g, int opaquei )
{
	CNOVRLockMutex( g->mutData );

	//This happens from within the render thread
	glBindBuffer( GL_ARRAY_BUFFER, g->nVBO );

	//XXX TODO: Consider streaming the data.
	glBufferData( GL_ARRAY_BUFFER, g->iStride*sizeof(float)*g->iVertexCount, g->pVertices, g->bDynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
	glVertexPointer( g->iStride, GL_FLOAT, 0, 0);

	CNOVRUnlockMutex( g->mutData );
}

void CNOVRVBOTaint( cnovr_vbo * g )
{
	CNOVRJobCancel( cnovrQPrerender, (cnovr_cb_fn)CNOVRVBOPerformUpload, (void*)g, 0 );
	CNOVRJobTack( cnovrQPrerender, (cnovr_cb_fn)CNOVRVBOPerformUpload, (void*)g, 0 );
}

void CNOVRVBODelete( cnovr_vbo * g )
{
	CNOVRLockMutex( g->mutData );
	CNOVRJobCancel( cnovrQPrerender, (cnovr_cb_fn)CNOVRVBOPerformUpload, (void*)g, 0 );
	glDeleteBuffers( 1, &g->nVBO );
	free( g->pVertices );
	CNOVRDeleteMutex( g->mutData );
}

void CNOVRVBOSetStride( cnovr_vbo * g, int stride )
{
	g->iStride = stride;
	CNOVRVBOTaint( g );
}

///////////////////////////////////////////////////////////////////////////////

static void CNOVRModelUpdateIBO( cnovr_model * m, int i )
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->nIBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ushort)*3, m->pIndices, GL_STATIC_DRAW);
	m->bTaintIndices = 0;
}


static int CNOVRModelDelete( cnovr_model * m )
{
	OGLockMutex( m->model_mutex );
	CNOVRJobCancel( cnovrQPrerender, (cnovr_cb_fn)CNOVRModelUpdateIBO, (void*)g, 0 );
	int i;
	for( i = 0; i < m->iGeos; i++ )
	{
		CNOVRVBODelete( m->pGeos[i] );
	}
	free( m->pIndices );
	glDeleteBuffers( 1, &m->nIBO );
}


static int CNOVRModelRender( cnovr_model * m )
{
	static cnovr_model * last_rendered_model = 0;
	if( m->bTaintIndices )
	{
		//This should almost never be changed.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->nIBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ushort)*3, m->pIndices GL_STATIC_DRAW);
	}
	if( m != last_rendered_model )
	{
		//Try binding any textures.
		int i;
		int count = m->iTextures;
		cnovr_texture * ts = m->pTextures;
		for( i = 0; i < count; i++ )
		{
			glActiveTexture( GL_TEXTURE0 + i );
			ts[i]->header.Render( ts[i] );
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->nIBO );
		count = m->iGeos;
		for( i = 0; i < count; i++ )
		{
			glBindBuffer( GL_ARRAY_BUFFER, m->pGeos[i].nVBO );
			glEnableVertexAttribArray(i);
		}
	}
	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, BUFFER_OFFSET(0));
}

cnovr_model * CNOVRModelCreate( int initial_indices, int num_vbos, int rendertype )
{
	cnovr_model * ret = malloc( sizeof( cnovr_model ) );
	memset( ret, 0, sizeof( cnovr_model ) );
	ret->Delete = (cnovrfn)CNOVRModelDelete;
	ret->Render = (cnovrfn)CNOVRModelRender;
	ret->header.Type = TYPE_GEOMETRY;
	glGenBuffers( 1, &ret->nVBO );
	ret->iIndexCount = initial_indices;
	if( initial_indices < 1 ) initial_indices = 1;
	ret->indices = malloc( initial_indices * sizeof( GLuint ) );
	ret->nRenderType = rendertype;
	ret->iMeshMarks = malloc( sizeof( int ) );
	ret->iMeshMarks[0] = 0;
	ret->nMeshes = 1;

	ret->pGeos = malloc( sizeof( cnovr_vbo * ) * 1 );
	ret->iGeos = 0;
	ret->pTextures = malloc( sizeof( cnovr_texture * ) );
	*ret->pTextures = 0;
	ret->iTextures = 0;
	ret->geofile = 0;
	ret->bTaintIndices = 0;
	ret->bIsLoading = 0;
	ret->iLastVertMark = 0;
	ret->model_mutex = OGCreateMutex(); 
}

void CNOVRModelSetNumVBOs( cnovr_model * m, int vbos )
{
	int i;
	for( i = 0; i < m->iGeos; i++ )
	{
		CNOVRVBODelete( m->pGeos[i] );
	}
	m->pGeos = realloc( m->pGeos, sizeof( cnovr_vbo * ) * vbos );
	m->iGeos = vbos

	for( i = 0; i < vbos; i++ )
	{
		m->pGeos[i] = CNOVRCreateVBO( 4, 0, 0, i );
	}
}

void CNOVRModelSetNumIndices( cnovr_model * m, uint32_t indices )
{
	m->iIndexCount = indices;
	m->pIndices = realloc( m->pIndices, sizeof( GLuint ) * indices );
}

void CNOVRModelResetMarks( cnovr_model * m )
{
	m->iMeshMarks = realloc( m->iMeshMarks, sizeof( uint32_t ) );
	m->nMeshes = 1;
	m->iMeshMarks[0] = 0;
	m->iLastVertMark = 0;
}

void CNOVRDelinateGeometry( cnovr_model * m )
{
	m->nMeshes++;
	m->iMeshMarks = realloc( m->iMeshMarks, sizeof( uint32_t ) * m->nMeshes );
	m->iMeshMarks[m->nMeshes] = iIndexCount;
}


void CNOVRModelTack( cnovr_model * m, int nindices, ...)
{
	OGLockMutex( m->model_mutex );
	int iIndexCount = m->iIndexCount;
	m->pIndices = realloc( m->pIndices, iIndexCount + nindices );
	uint32_t * pIndices = m->pIndices + iIndexCount;
	int i;
	va_list argp;
	va_start( argp, nverts );
	for( i = 0; i < nindices; i++ )
	{
		pIndices[i] = va_arg(argp, int);
	}
	va_end( argp );
	m->iIndexCount = m->iIndexCount + nindices;
	m->bTaintIndices = 1;
	OGUnockMutex( m->model_mutex );
}

void CNOVRModelTackv( cnovr_model * m, int nindices, uint32_t * indices )
{
	OGLockMutex( m->model_mutex );
	int iIndexCount = m->iIndexCount;
	m->pIndices = realloc( m->pIndices, iIndexCount + nindices );
	uint32_t * pIndices = m->pIndices + iIndexCount;
	int i;
	for( i = 0; i < nindices; i++ )
	{
		pIndices[i] = indices[i];
	}
	m->iIndexCount = m->iIndexCount + nindices;
	m->bTaintIndices = 1;
	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelMakeCube( cnovr_model * m, float sx, float sy, float sz )
{
	CNOVRModelSetNumVBOs( m, 3 );
	//Bit pattern. 0 means -1, 1 means +1 on position.
	//n[face] n[+-](inverted)  v1[xyz] v2[xyz] v3[xyz];; TC can be computed from table based on N
	//XXX TODO: Check texture coord correctness.
	static const uint16_t trideets[] = {
		0b000000001011,
		0b000000011010,
		0b001111100110,
		0b001100111101,
		0b010101000100,
		0b010101001000,
		0b011111110010,
		0b011111010011,
		0b100110100000,
		0b100110000010,
		0b101011001101,
		0b101111011101,
	};
	OGLockMutex( m->model_mutex );
	if( m->iGeos != 3 )
	{
		CNOVRModelSetNumVBOs( m, 3 );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
	}

	int i;
	{
		uint32_t * indices = m->pIndices;
		for( i = 0; i < 36; i++ ) 
		{
			CNOVRModelTack( m, i + m->iLastVertMark );
		}
	}

	{
		//float * points = m->pGeos[0]->pVertices;
		//float * texcoord = m->pGeos[1]->pVertices;
		//float * normals = m->pGeos[2]->pVertices;

		for( i = 0; i < 12; i++ )
		{
			uint16_t key = trideets[i];
			int normaxis = (key>>10)&3;
			float normplus = ((key>>9)&1)?1:-1;
			int tcaxis1 = (normaxis+1)%3;
			int tcaxis2 = (tcaxis1+1)%3;
			int vkeys[3] = { 
				(key >> 6)&7,
				(key >> 3)&7,
				(key >> 0)&7 };
			int j;
			for( j = 0; j < 3; j++ )
			{
				float stage[4];
				stage[0] = (vkey[j]&4)?1:-1;
				stage[1] = (vkey[j]&2)?1:-1;
				stage[2] = (vkey[j]&1)?1:-1;
				stage[3] = 1;
				CNOVRVBOTackv( m->pGeos[0], 4, stage );

				stage[0] = points[ti+tcaxis1];
				stage[1] = points[ti+tcaxis2];
				stage[2] = 1;
				stage[3] = m->nMeshes;
				CNOVRVBOTackv( m->pGeos[1], 4, stage );

				stage[0] = (normaxis==0)?normplus:0;
				stage[1] = (normaxis==1)?normplus:0;
				stage[2] = (normaxis==2)?normplus:0;
				stage[3] = 0;
				CNOVRVBOTackv( m->pGeos[2], 4, stage );
			}
		}
	}

	m->iLastVertMark += 36;

	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );
	m->bTaintIndices = 1;

	CNOVRDelinateGeometry( m );
	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelMakeMesh( cnovr_model * m, int rows, int cols, float sx, float sy )
{
	//Copied from Spreadgine.
	int i;
	int x, y;
	int c = w * h;
	int v = (w+1)*(h+1);

	OGLockMutex( m->model_mutex );

	if( m->iGeos != 3 )
	{
		CNOVRModelSetNumVBOs( m, 3 );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
	}
	CNOVRModelSetNumIndices( m, 6*c );

	{
		uint32_t * indices = m->pIndices;
		for( y = 0; y < h; y++ )
		for( x = 0; x < w; x++ )
		{
			int i = x + y * w;
			int k = m->iLastVertMark;
			CNOVRModelTack( m, k + x + y * (w+1) );
			CNOVRModelTack( m, k + (x+1) + y * (w+1) );
			CNOVRModelTack( m, k + (x+1) + (y+1) * (w+1) );
			CNOVRModelTack( m, k + (x) + (y) * (w+1) );
			CNOVRModelTack( m, k + (x+1) + (y+1) * (w+1) );
			CNOVRModelTack( m, k + (x) + (y+1) * (w+1) );
		}
	}

	{
		float stage[4];
		float stagen[4];

		stagen[0] = 0;
		stagen[1] = 0;
		stagen[2] = -1;
		stagen[3] = 0;

		for( y = 0; y <= h; y++ )
		for( x = 0; x <= w; x++ )
		{
			stage[0] = x/(float)w;
			stage[1] = y/(float)h;
			stage[2] = 1;
			stage[3] = m->nMeshes;

			CNOVRVBOTackv( m->pGeos[0], 4, stage );
			CNOVRVBOTackv( m->pGeos[1], 4, stage );
			CNOVRVBOTackv( m->pGeos[2], 4, stagen );
		}
	}

	m->iLastVertMark += (w+1)*(h+1);
	m->bTaintIndices = 1;
	CNOVRDelinateGeometry( m );
	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );

	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelApplyTextureFromFileAsync( cnovr_model * m, const char * sTextureFile )
{
	if( m->iTextures == 0 )
	{
		*m->pTextures[0] = CNOVRTextureCreate( 1, 1, 4 );
		m->iTextures = 1;
	}
	CNOVRTextureLoadFileAsync( m->pTextures[0], sTextureFile );
}

void CNOVRModelRenderWithPose( cnovr_model * m, cnovr_pose * pose )
{
	pose_to_matrix44( cnovrstate.mModel, pose );
	m->header.Render( m );
}

int  CNOVRModelCollide( cnovr_model * m, const cnovr_point3d start, const cnovr_vec3d direction, cnovr_collide_results * r )
{
	if( m->iGeos == 0 ) return -1;
	int iMeshNo = 0;
	//Iterate through all this.
	float * vpos = pGeos[0]->pVertices;
	int stride = pGeos[0]->iStride;
	int i;
	for( i = 0; i < m->nMeshes; i++ )
	{
		int meshStart = m->iMeshMarks[i] 
		int meshEnd = (i == m->nMeshes-1 ) ? m->iIndexCount : m->iMeshMarks[i+1];
		int j;
		for( j = meshStart; j < meshEnd; j+=3 )
		{
			int i0 = j+0;
			int i1 = j+1;
			int i2 = j+2;
			//i0..2 are the indices we will be verting.
			float * v0 = &vpos[i0*stride];
			float * v1 = &vpos[i1*stride];
			float * v2 = &vpos[i2*stride];

			float v10[3];
			float v21[3];
			float v02[3];
			float N[3];
			sub3d( v10, v1, v0 );
			sub3d( v21, v2, v1 );
			sub3d( v02, v0, v2 );
			{
				float v20[3];
				cross3d( N, v20, v10 );
				sub3d( v20, v2, v0 );
			}
			normalize3d( N, N );
			float D = dot3d(N, v0);
			float t = -( dot( N, start ) + D) / dot( N, direction ); 
			float Phit[3];
			scale3d( Phit, direction, t );
			add3d( Phit, start );
			float C0[3];
			float C1[3];
			float C2[3];
			sub3d( C0, Phit, v0 );
			sub3d( C1, Phit, v1 );
			sub3d( C2, Phit, v2 );
			crossProduct( C0, v10, C0 );
			crossProduct( C1, v21, C1 );
			crossProduct( C2, v02, C2 );
			if( dot3d( N, C0 ) < 0 ||
				dot3d( N, C1 ) < 0 ||
				dot3d( N, C2 ) < 0 ) continue;

			//Else: We have a hit.
			if( t < r->t )
			{
				r->t = t;
				r->whichmesh = i;
				r->whichvert = j;
				copy3d( r->collidepos, Phit );

				//Now, do barycentric coordinates to get the rest of the info here.
				//XXX TODO TODO.
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//Model Loaders

void CNOVRModelLoadFromFileAsync( cnovr_model * m, const char * filename )
{
	
}

void CNOVRModelLoadRenderModelAsync( cnovr_model * m, const char * pchRenderModelName )
{
	
}

