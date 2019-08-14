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

static void CNOVRTexturePrerender( cnovr_texture * ths )
{
	if( ths->bTaintData )
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
}

//Defaults to a 1x1 px texture.
cnovr_texture CNOVRTextureCreate( int initw, int inith, int initchan )
{
	cnovr_texture * ret = malloc( sizeof( cnovr_rf_buffer ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->header.Delete = (cnovrfn)CNOVRTextureDelete;
	ret->header.Render = (cnovrfn)CNOVRTextureRender;
	ret->header.Prerender = (cnovrfn)CNOVRTexturePrerender;
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
	OGUnlockMutex( tex->mutProtect );
}



///////////////////////////////////////////////////////////////////////////////





