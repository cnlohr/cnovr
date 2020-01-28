// Copyright 2019 <>< Charles Lohr licensable under the MIT/X11 or NewBSD licenses.

#include <cnovrparts.h>
#include <cnovr.h>
#include <cnovrutil.h>
#include <chew.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stb_image.h>
#include <stb_include_custom.h>
#include <cnovrtccinterface.h>
#include <stretchy_buffer.h>
#include <cnrbtree.h>

//XXX Overall TODO: Replace more FreeLater's with frees

static void parts_delete_callback( void * tag, void * opaquev )
{
	cnovr_base * b = (cnovr_base*)opaquev;
	b->header->Delete( b );
}

void CNOVRDeleteBase( cnovr_base * b )
{
	if( !b ) return;
	//XXX Tricky: Use -1 tag to prevent accidental task deletion.
	CNOVRJobTack( cnovrQPrerender, parts_delete_callback, (void*)-1, b, 0 );
}


static void CNOVRRenderFrameBufferDelete( cnovr_rf_buffer * ths )
{
	//Tricky - render and resolve may be the same if no multisampling is used.
	if( ths->nResolveFramebufferId && ths->nResolveFramebufferId != ths->nRenderFramebufferId ) glDeleteFramebuffers( 1, &ths->nResolveFramebufferId );
	if( ths->nRenderFramebufferId ) glDeleteFramebuffers( 1, &ths->nRenderFramebufferId );
	if( ths->nResolveTextureId ) glDeleteTextures( 1, &ths->nResolveTextureId );
	if( ths->nDepthBufferId ) glDeleteRenderbuffers( 1, &ths->nDepthBufferId );
	if( ths->nColorBufferId ) glDeleteRenderbuffers( 1, &ths->nColorBufferId );
	if( ths->nRenderTextureId ) glDeleteTextures( 1, &ths->nRenderTextureId );
	CNOVRListDeleteTag( ths );

	CNOVRFreeLater( ths );
}

cnovr_header cnovr_rf_buffer_header = {
	(cnovrfn)CNOVRRenderFrameBufferDelete,
	0, //Render
	TYPE_RFBUFFER,
};

cnovr_rf_buffer * CNOVRRFBufferCreate( int nWidth, int nHeight, int multisample )
{
	cnovr_rf_buffer * ret = malloc( sizeof( cnovr_rf_buffer ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->base.header = &cnovr_rf_buffer_header;
	ret->base.tccctx = TCCGetTag();

	ret->multisample = multisample;
	printf( "CNOVRRFBufferCreate %d (%d,%d)\n", multisample, nWidth, nHeight );

	//XXX TODO: Figure out why we can't use depth buffers with multisamples.
	int texmul = multisample?GL_TEXTURE_2D_MULTISAMPLE:GL_TEXTURE_2D;


	glGenFramebuffers(1, &ret->nRenderFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, ret->nRenderFramebufferId);

	glGenRenderbuffers(1, &ret->nDepthBufferId);
	glBindRenderbuffer(GL_RENDERBUFFER, ret->nDepthBufferId);
	if( multisample )
	{
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, GL_DEPTH_COMPONENT, nWidth, nHeight );
	}
	else
	{
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, nWidth, nHeight );
	}
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,	ret->nDepthBufferId );
	glGenTextures(1, &ret->nRenderTextureId );
	glBindTexture(texmul, ret->nRenderTextureId );
	if( !multisample )
	{
		glTexParameterf(texmul, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(texmul, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(texmul, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(texmul, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	//glTexParameteri(texmul, GL_GENERATE_MIPMAP, GL_TRUE);

	if( multisample )
	{
		glTexImage2DMultisample(texmul, multisample, GL_RGBA8, nWidth, nHeight, GL_TRUE);
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	}

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texmul, ret->nRenderTextureId, 0);


//	glGenRenderbuffers(1, &ret->nColorBufferId);
//	glBindRenderbuffer(GL_RENDERBUFFER, ret->nColorBufferId);
//	if( multisample )
//		glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample, GL_RGBA8, nWidth, nHeight );
//	else
//		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, nWidth, nHeight );
//	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, ret->nColorBufferId );

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf( "Warning bad framebuffer setup (Render)\n" );
	}

	if( !multisample )
	{
		ret->nResolveFramebufferId = 0;
		ret->nResolveTextureId = ret->nRenderTextureId;
	}
	else
	{
		glGenFramebuffers(1, &ret->nResolveFramebufferId );
		glBindFramebuffer(GL_FRAMEBUFFER, ret->nResolveFramebufferId);
		glGenTextures(1, &ret->nResolveTextureId );
		glBindTexture(GL_TEXTURE_2D, ret->nResolveTextureId );
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ret->nResolveTextureId, 0);
	}

	// check FBO status
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		printf( "Warning: Bad framebuffersetup (Resolve)\n" );
		CNOVRRenderFrameBufferDelete( ret );
		return 0;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	
	if( !multisample )
	{
		ret->resolveshader = 0;
		ret->resolvegeo = 0;
	}
	else
	{
		char stbuf[128];
		sprintf( stbuf, "#define MULTISAMPLES %d\n", multisample );
		ret->resolveshader = CNOVRShaderCreateWithPrefix( "resolve", stbuf );
		ret->resolvegeo = CNOVRModelCreate( 0, GL_TRIANGLES );
		CNOVRModelAppendMesh( ret->resolvegeo, 1, 1, 0, (cnovr_point3d){ 1, 1, 0 }, 0, 0 );
	}
	return ret;
}


void CNOVRFBufferActivate( cnovr_rf_buffer * b )
{
	if( CNOVRCheck() ) ovrprintf( "PRE ACTIVAT\n" );
	b->origw = cnovrstate->iRTWidth;
	b->origh = cnovrstate->iRTHeight;
	int w = cnovrstate->iRTWidth = b->width;
	int h = cnovrstate->iRTHeight = b->height;
	if( b->multisample )  glEnable( GL_MULTISAMPLE );
	glBindFramebuffer( GL_FRAMEBUFFER, b->nRenderFramebufferId );
 	glViewport(0, 0, w, h );
	if( CNOVRCheck() ) ovrprintf( "POST ACTIVATE\n" );
}

void CNOVRFBufferDeactivate( cnovr_rf_buffer * b )
{
	if( CNOVRCheck() ) ovrprintf( "PRE DEACTIVATE\n" );
 	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	int w = cnovrstate->iRTWidth = b->origw;
	int h = cnovrstate->iRTHeight = b->origh;
 	glViewport(0, 0, w, h );
	if( CNOVRCheck() ) ovrprintf( "POST DEACTIVATE\n" );
}

void CNOVRFBufferBlitResolve( cnovr_rf_buffer * b )
{
	if( CNOVRCheck() ) ovrprintf( "PRE RESOLVE\n" );

#if 0
	//Broken???!?
	glBindFramebuffer( GL_FRAMEBUFFER, 0);
	if( b->multisample ) glDisable( GL_MULTISAMPLE );
	glBindFramebuffer(GL_READ_FRAMEBUFFER, b->nRenderFramebufferId );
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, b->nResolveFramebufferId );
	glBlitFramebuffer(0, 0, b->width, b->height, 0, 0, b->width, b->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
 	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );	
#else

	if( b->multisample )
	{
		//glEnable( GL_MULTISAMPLE );
		glBindFramebuffer( GL_FRAMEBUFFER, b->nResolveFramebufferId);
		glActiveTextureCHEW( GL_TEXTURE0 );
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, b->nRenderTextureId );
		if( CNOVRCheck() ) ovrprintf( "MIDDLE RESOLVE\n" );
		glDisable( GL_BLEND ); // XXX TODO: Want to be in for a wild ride?  With multisample on in mixed reality, enable blending here! HAHAHAHAH
		CNOVRRender( b->resolveshader );
		if( CNOVRCheck() ) ovrprintf( "MIDDLE1 RESOLVE\n" );
		if( CNOVRCheck() ) ovrprintf( "MIDDLE2 RESOLVE\n" );
		CNOVRRender( b->resolvegeo );
	}
#endif

	glBindFramebuffer( GL_FRAMEBUFFER, 0);

	int w = cnovrstate->iRTWidth = b->origw;
	int h = cnovrstate->iRTHeight = b->origh;
 	glViewport(0, 0, w, h );
	if( CNOVRCheck() ) ovrprintf( "POST RESOLVE\n" );
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

#if 0
static void CNOVRShaderFileClearWatchlist( cnovr_shader * s )
{
	struct watchlist * w = s->tempwl;
	while( w )
	{
		struct watchlist * next = w->next;
		free( w );
		w = next;
	}
	s->tempwl = 0;
}
#endif

static void CNOVRShaderDelete( cnovr_shader * ths )
{
	CNOVRFileTimeRemoveTagged( ths, 1 );
	CNOVRListDeleteTag( ths );
	CNOVRJobCancelAllTag( ths, 1 );
	if( ths->nShaderID ) glDeleteProgram( ths->nShaderID );
	if( ths->prefix ) free( ths->prefix );
	//CNOVRShaderFileClearWatchlist( ths );
	CNOVRFreeLater( ths->shaderfilebase );
	CNOVRFreeLater( ths );
}

static GLuint CNOVRShaderCompilePart( cnovr_shader * ths, GLuint shader_type, const char * shadername, const char * compstr )
{
	GLuint nShader = glCreateShader( shader_type );
	glShaderSource( nShader, 1, &compstr, NULL );
	glCompileShader( nShader );
	GLint vShaderCompiled = GL_FALSE;

	glGetShaderiv( nShader, GL_COMPILE_STATUS, &vShaderCompiled );
	if ( vShaderCompiled != GL_TRUE )
	{
		CNOVRAlert( ths->base.tccctx, 1, "Unable to compile shader: %s\n", shadername );
		int retval;
		glGetShaderiv( nShader, GL_INFO_LOG_LENGTH, &retval );
		if ( retval > 1 ) {
			char * log = (char*)malloc( retval );
			glGetShaderInfoLog( nShader, retval, NULL, log );
			CNOVRAlert( ths->base.tccctx, 1, "%s\n", log );
			free( log );
		}

		glDeleteShader( nShader );
		return 0;
	}
	return nShader;
}

static void CNOVRShaderFileChange( void * tag, void * opaquev );

static void CNOVRShaderFileTackInclude( void * opaque, const char * filename )
{
	cnovr_shader * s = (cnovr_shader*)opaque;
	//int fnamelen = strlen( filename );
	CNOVRFileTimeAddWatch( filename, CNOVRShaderFileChange, s, 0 );
	//struct watchlist * new = malloc( sizeof( struct watchlist*) + fnamelen + 9 );
	//memcpy( new->watchfile, filename, fnamelen+1 );
	//new->next = s->tempwl;
	//s->tempwl = new;
}

static void CNOVRShaderFileChangePrerender( void * tag, void * opaquev )
{
	//Re-load shader
	cnovr_shader * ths = (cnovr_shader*)tag;
	//Careful: Need to re-try in case a program is still writing.

	GLuint nGeoShader  = 0;
	GLuint nFragShader = 0;
	GLuint nVertShader = 0;

	char stfbGeo[CNOVR_MAX_PATH];
	char stfbFrag[CNOVR_MAX_PATH];
	char stfbVert[CNOVR_MAX_PATH];
	char * filedataGeo = 0;
	char * filedataFrag = 0;
	char * filedataVert = 0;

	char includeerrors1[256];
	char includeerrors2[256];
	includeerrors1[0] = 0;
	includeerrors2[0] = 0;
	//printf( "THS: %p\n", ths );
	sprintf( stfbGeo, "%s.geo", ths->shaderfilebase );
	char * found = CNOVRFileSearch( stfbGeo ); if( found ) strcpy( stfbGeo, found );
	filedataGeo = stb_include_file( stfbGeo, ths->prefix, "assets", includeerrors1, CNOVRShaderFileTackInclude, tag );
	//XXX TODO: Do we care about odd errors on geo?
	sprintf( stfbFrag, "%s.frag", ths->shaderfilebase );
	found = CNOVRFileSearch( stfbFrag ); if( found ) strcpy( stfbFrag, found );
	filedataFrag = stb_include_file( stfbFrag, ths->prefix, "assets", includeerrors2, CNOVRShaderFileTackInclude, tag );
	sprintf( stfbVert, "%s.vert", ths->shaderfilebase );
	found = CNOVRFileSearch( stfbVert ); if( found ) strcpy( stfbVert, found );
	filedataVert = stb_include_file( stfbVert, ths->prefix, "assets", includeerrors2, CNOVRShaderFileTackInclude, tag );

	if( includeerrors2[0] )
	{
		CNOVRAlert( ths->base.tccctx, 1, "Shader preprocessor errors: %s\n", includeerrors2 );
		return;		
	}

	if( !filedataFrag || !filedataVert )
	{
		CNOVRAlert( ths->base.tccctx, 1, "Unable to open vert/frag in shader: %s\n", ths->shaderfilebase );
		return;
	}

	if( filedataGeo ) nGeoShader = CNOVRShaderCompilePart( ths, GL_GEOMETRY_SHADER, stfbGeo, filedataGeo );
	nFragShader = CNOVRShaderCompilePart( ths, GL_FRAGMENT_SHADER, stfbFrag, filedataFrag );
	nVertShader = CNOVRShaderCompilePart( ths, GL_VERTEX_SHADER, stfbVert, filedataVert );

	if( filedataVert ) free( filedataVert );
	if( filedataFrag ) free( filedataFrag );
	if( filedataGeo ) free( filedataGeo );

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
		if( filedataGeo )
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
			CNOVRAlert( ths->base.tccctx, 1, "Shader linking failed: %s\n", ths->shaderfilebase );
			int retval;
			glGetShaderiv( unProgramID, GL_INFO_LOG_LENGTH, &retval );
			if ( retval > 1 ) {
				char * log = (char*)malloc( retval );
				glGetProgramInfoLog( unProgramID, retval, NULL, log );
				CNOVRAlert( ths->base.tccctx, 1, "%s\n", log );
				free( log );
			}
			else
			{
				CNOVRAlert( ths->base.tccctx, 1, "No message\n" );
			}
			glDeleteProgram( unProgramID );
			unProgramID = 0;
			compfail = true;
		}
	}
	else
	{
		CNOVRAlert( ths->base.tccctx, 1, "Shader compilation failed: %s\n", ths->shaderfilebase );
	}

	if ( unProgramID )
	{
		if ( ths->nShaderID )
		{
			//CNOVRAlert( ths->base.tccctx, 3, "Compile OK: [%p] %s\n", ths, ths->shaderfilebase );
			//Note: If we got here, we were successful. 
			glDeleteProgram( ths->nShaderID );
		}
		ths->nShaderID = unProgramID;
	}

	if( nGeoShader )  glDeleteShader( nGeoShader );
	if( nFragShader ) glDeleteShader( nFragShader );
	if( nVertShader ) glDeleteShader( nVertShader );
}

static void CNOVRShaderFileChange( void * tag, void * opaquev )
{
	//printf( "CNOVRShaderFileChange (%p %p)\n", tag, opaquev );
	//0 = don't recompile if a recompile operation is already pending.
	CNOVRJobTack( cnovrQPrerender, CNOVRShaderFileChangePrerender, tag, opaquev, 0 );
}

static void CNOVRShaderRender( cnovr_shader * ths )
{
	int shdid = ths->nShaderID;
	if( !shdid ) { return; }
	glUseProgram( shdid );
	CNOVRShaderLoadedSetUniformsInternal();
}

cnovr_header cnovr_shader_header = {
	(cnovrfn)CNOVRShaderDelete, //Delete
	(cnovrfn)CNOVRShaderRender, //Render
	TYPE_SHADER,
};

cnovr_shader * CNOVRShaderCreate( const char * shaderfilebase )
{
	return CNOVRShaderCreateWithPrefix( shaderfilebase, 0 );
}

cnovr_shader * CNOVRShaderCreateWithPrefix( const char * shaderfilebase, const char * prefix )
{
	cnovr_shader * ret = malloc( sizeof( cnovr_shader ) );
	memset( ret, 0, sizeof( *ret ) );
	ret->base.header = &cnovr_shader_header;
	ret->base.tccctx = TCCGetTag();
	ret->shaderfilebase = strdup( shaderfilebase );
	ret->prefix = prefix?strdup(prefix):0;

	char stfb[CNOVR_MAX_PATH];
	sprintf( stfb, "%s.geo", shaderfilebase );
	char * found = CNOVRFileSearch( stfb );
	CNOVRFileTimeAddWatch( found, CNOVRShaderFileChange, ret, 0 );
	sprintf( stfb, "%s.frag", shaderfilebase );
	found = CNOVRFileSearch( stfb );
	CNOVRFileTimeAddWatch( stfb, CNOVRShaderFileChange, ret, 0 );
	sprintf( stfb, "%s.vert", shaderfilebase );
	found = CNOVRFileSearch( stfb );
	CNOVRFileTimeAddWatch( stfb, CNOVRShaderFileChange, ret, 0 );
	CNOVRShaderFileChange( ret, 0 );

	return ret;
}


//////////////////////////////////////////////////////////////////////////////

static void CNOVRTextureLoadFileTask( void * tag, void * opaquev )
{
	cnovr_texture * t = (cnovr_texture*)tag;
	OGLockMutex( t->mutProtect );
	char * localfn = strdup( t->texfile );
	OGUnlockMutex( t->mutProtect );

	int x, y, chan;
	const char * ffn = CNOVRFileSearch( localfn );
	chan = 4;
	x = 0;
	y = 0;
	stbi_uc * data = stbi_load( ffn, &x, &y, &chan, 4 );
	chan = 4;	//XXX UGH No idea why stbi_load does weird stuff if we don't specifically request 4.
	CNOVRFileTimeAddWatch( ffn, CNOVRTextureLoadFileTask, tag, 0 );
	free( localfn );
	if( data )
	{
		CNOVRTextureLoadDataAsync( t, x, y, chan, 0, data );
		t->bLoading = 0;
	}
	else
	{
		ovrprintf( "WARNING: stbi_load( %s (%s), ... ) failed. Is it saving? Trying again.\n", ffn, t->texfile );
		CNOVRJobCancel( cnovrQAsync, CNOVRTextureLoadFileTask, t, 0, 0 );
		CNOVRJobTack( cnovrQAsync, CNOVRTextureLoadFileTask, t, 0, 1 ); //If one's already going, let it finish.

	}

}

static void CNOVRTextureUploadCallback( void * vths, void * dump )
{
	cnovr_texture * t = (cnovr_texture*)vths;
	OGLockMutex( t->mutProtect );
	t->bTaintData = 0;

	if( !t->nTextureId )
	{
		glGenTextures( 1, &t->nTextureId );
		glBindTexture( GL_TEXTURE_2D, t->nTextureId );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}


	glBindTexture( GL_TEXTURE_2D, t->nTextureId );

	glTexImage2D( GL_TEXTURE_2D,
		0,
		t->nInternalFormat,
		t->width,
		t->height,
		0,
		t->nFormat,
		t->nType,
		t->data );

	if( t->bCalculateMipMaps )
	{
		glGenerateMipmapCHEW(GL_TEXTURE_2D);
	}
	else
	{
		//
	}
	glBindTexture( GL_TEXTURE_2D, 0 );

	OGUnlockMutex( t->mutProtect );
}

static void CNOVRTextureDelete( cnovr_texture * ths )
{
	OGLockMutex( ths->mutProtect );
	CNOVRListDeleteTag( ths );
	//In case any file changes are being watched.
	CNOVRFileTimeRemoveTagged( ths, 1 );
	CNOVRJobCancelAllTag( ths, 1 );
	if( ths->nTextureId )
	{
		glDeleteTextures( 1, &ths->nTextureId );
	}

	if( ths->data ) free( ths->data );
	if( ths->texfile ) free( ths->texfile );
	OGDeleteMutex( ths->mutProtect );
	CNOVRFreeLater( ths );
}

static void CNOVRTextureRender( cnovr_texture * ths )
{
	glBindTexture( GL_TEXTURE_2D, ths->nTextureId );
}


cnovr_header cnovr_texture_header = {
	(cnovrfn)CNOVRTextureDelete, //Delete
	(cnovrfn)CNOVRTextureRender, //Render
	TYPE_TEXTURE,
};

//Defaults to a 1x1 px texture.
cnovr_texture * CNOVRTextureCreate( int initw, int inith, int initchan )
{
	cnovr_texture * ret = malloc( sizeof( cnovr_texture ) );
	memset( ret, 0, sizeof( cnovr_texture ) );
	ret->base.header = &cnovr_texture_header;
	ret->base.tccctx = TCCGetTag();
	ret->texfile = 0;

	ret->mutProtect = OGCreateMutex();

	ret->nTextureId = 0;

	ret->width = 1;
	ret->height = 1;
	ret->channels = 4;
	ret->data = malloc( 4 );
	ret->bTaintData = 0;
	ret->bLoading = 0;
	ret->bFileChangeFlag = 0;
	memset( ret->data, 255, 4 );


	return ret;
}

int CNOVRTextureLoadFileAsync( cnovr_texture * tex, const char * texfile )
{
	OGLockMutex( tex->mutProtect );
	if( tex->texfile ) free( tex->texfile );
	tex->texfile = strdup( texfile );
	tex->bLoading = 1;
	CNOVRJobCancel( cnovrQAsync, CNOVRTextureLoadFileTask, tex, 0, 0 );
	CNOVRJobTack( cnovrQAsync, CNOVRTextureLoadFileTask, tex, 0, 1 ); //If one's already going, let it finish.
	OGUnlockMutex( tex->mutProtect );
	return 0;
}

static void InternalCNOVRTextureLoadSetup(  cnovr_texture * tex, int w, int h, int chan, int is_float )
{
	tex->width = w;
	tex->height = h;
	tex->channels = chan;
	static const int channelmapF[] = { 0, GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F };
	static const int channelmapI[] = { 0, GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
	static const int channelmapB[] = { 0, GL_RED, GL_RG, GL_RGB, GL_RGBA };

	if( is_float )
		tex->nInternalFormat = channelmapF[chan];
	else
		tex->nInternalFormat = channelmapI[chan];
	tex->nFormat = channelmapB[chan];
	tex->nType = is_float?GL_FLOAT:GL_UNSIGNED_BYTE;
	tex->bTaintData = 1;
}

int CNOVRTextureLoadDataNow( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data, int data_permanant )
{
	OGLockMutex( tex->mutProtect );
	InternalCNOVRTextureLoadSetup( tex, w, h, chan, is_float );

	if( tex->data ) free( tex->data );
	tex->data = data;

	CNOVRTextureUploadCallback( tex, 0 );

	//If data is permanant, we don't have to worry about deleting it.
	if( data_permanant )
	{
		tex->data = 0;
	}

	OGUnlockMutex( tex->mutProtect );
	return 0;
}

int CNOVRTextureLoadDataAsync( cnovr_texture * tex, int w, int h, int chan, int is_float, void * data )
{
	OGLockMutex( tex->mutProtect );

	//We don't want to confuse the second part of the loader.
	CNOVRJobCancel( cnovrQPrerender, CNOVRTextureUploadCallback, tex, 0, 1 );

	if( tex->data && data != tex->data ) free( tex->data );
	tex->data = data;

	InternalCNOVRTextureLoadSetup( tex, w, h, chan, is_float );

	CNOVRJobTack( cnovrQPrerender, CNOVRTextureUploadCallback, tex, 0, 1 );
	OGUnlockMutex( tex->mutProtect );
	return 0;
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
	ret->bDynamic = bDynamic;
	ret->mutData = OGCreateMutex();
	ret->nVBO = 0;

	return ret;
}


void CNOVRVBOTackv( cnovr_vbo * g, int nverts, float * v )
{
	OGLockMutex( g->mutData );
	int stride = g->iStride;
	g->pVertices = realloc( g->pVertices, (g->iVertexCount+1)*stride*sizeof(float) );
	float * verts = g->pVertices + (g->iVertexCount*stride);
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
	OGUnlockMutex( g->mutData );
}

void CNOVRVBOTack( cnovr_vbo * g,  int nverts, ... )
{
	OGLockMutex( g->mutData );
	int stride = g->iStride;
	g->pVertices = realloc( g->pVertices, (g->iVertexCount+1)*stride*sizeof(float) );
	float * verts = g->pVertices + (g->iVertexCount*stride);
	va_list argp;
	va_start( argp, nverts );
	int i;
	if( nverts > stride ) nverts = stride;
	for( i = 0; i < nverts; i++ )
	{
		verts[i] = va_arg(argp, double);
	}
	for( ; i < stride; i++ )
	{
		verts[i] = 0.0;
	}
	g->iVertexCount++;
	va_end( argp );
	OGUnlockMutex( g->mutData );
}

static void CNOVRVBOPerformUpload( void * gv, void * dump )
{
	cnovr_vbo * g = (cnovr_vbo *)gv;

	OGLockMutex( g->mutData );

	if( !g->nVBO ) glGenBuffers( 1, &g->nVBO );

	//This happens from within the render thread
	glBindBuffer( GL_ARRAY_BUFFER, g->nVBO );

	//XXX TODO: Consider streaming the data.
	glBufferData( GL_ARRAY_BUFFER, g->iStride*sizeof(float)*g->iVertexCount, g->pVertices, g->bDynamic?GL_DYNAMIC_DRAW:GL_STATIC_DRAW);
	glVertexPointer( g->iStride, GL_FLOAT, 0, 0);
	//glVertexAttribPointer( 0, g->iStride, GL_FLOAT, 0, g->iStride, g->pVertices );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	g->bIsUploaded = 1;

#if 0
	int i;
	for( i = 0; i < g->iVertexCount; i++ )
	{
		int j;
		for( j = 0; j < g->iStride; j++ )
		{
			printf( "%f ", g->pVertices[j+i*g->iStride] );
		}
		printf( "\n" );
	}
#endif
	if( CNOVRCheck() ) ovrprintf( "Problem in VBO Upload check\n" );

	OGUnlockMutex( g->mutData );
}

void CNOVRVBOTaint( cnovr_vbo * g )
{
	CNOVRJobCancel( cnovrQPrerender, CNOVRVBOPerformUpload, (void*)g, 0, 0 );
	CNOVRJobTack( cnovrQPrerender, CNOVRVBOPerformUpload, (void*)g, 0, 1 );
}

void CNOVRVBODelete( cnovr_vbo * g )
{
	//Not a normal object.
	CNOVRJobCancelAllTag( (void*)g, 1 );
	OGLockMutex( g->mutData );
	if( g->nVBO ) glDeleteBuffers( 1, &g->nVBO );
	CNOVRFreeLater( g->pVertices );
	OGDeleteMutex( g->mutData );
	CNOVRFreeLater( g );
}

void CNOVRVBOSetStride( cnovr_vbo * g, int stride )
{
	g->iStride = stride;
}

///////////////////////////////////////////////////////////////////////////////

static void CNOVRModelUpdateIBO( void * vm, void * dump )
{
//	printf( "START MODEL UPATE\n" );
	cnovr_model * m = (cnovr_model *)vm;
	OGLockMutex( m->model_mutex );
	if( m->nIBO < 0 ) glGenBuffers( 1, (GLuint*)&m->nIBO );
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->nIBO);
//	printf( "Updating IBO: %d\n", m->iIndexCount );
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m->pIndices[0])*m->iIndexCount, m->pIndices, GL_STATIC_DRAW);	//XXX TODO Make this tunable.
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	OGUnlockMutex( m->model_mutex );
	m->bIsUploaded = 1;
//	printf( "STOP MODEL UPDATE\n" );
#if 0
	printf( "### IBO UPATE INDICES: %ld\n", sizeof(m->pIndices[0])*m->iIndexCount );
	int i;
//	for( i = 0; i < m->iIndexCount ; i++ )
//	{
//		printf( "%d\n", m->pIndices[i] );
//	}
#endif
}

void CNOVRModelTaintIndices( cnovr_model * vm )
{
	vm->iMeshMarks[vm->nMeshes] = vm->iIndexCount+1;
	CNOVRJobTack( cnovrQPrerender, CNOVRModelUpdateIBO, (void*)vm, 0, 1 );	
}

static void CNOVRModelDelete( cnovr_model * m )
{
	OGLockMutex( m->model_mutex );
	CNOVRListDeleteTag( m );
	CNOVRJobCancelAllTag( (void*)m, 1 );
	int i;
	for( i = 0; i < m->iGeos; i++ )
	{
		CNOVRVBODelete( m->pGeos[i] );
	}
	CNOVRFreeLater( m->pGeos );
	if( m->sMeshMarks )
	{
		for( i = 0; i < m->nMeshes; i++ )
		{
			if( m->sMeshMarks[i] ) free( m->sMeshMarks[i] );
		}
		free( m->sMeshMarks );
	}
	if( m->iMeshMarks )
	{
		free( m->iMeshMarks );
	}
	for( i = 0; i < m->iTextures; i++ )
	{
		CNOVRDelete( m->pTextures[i] );
	}

	CNOVRFreeLater( m->pIndices );
	if( m->geofile ) CNOVRFreeLater( m->geofile );
	if( m->nIBO >= 0 ) glDeleteBuffers( 1, (GLuint*)&m->nIBO );
	OGDeleteMutex( m->model_mutex );
	CNOVRFreeLater( m );
}


static void CNOVRModelRender( cnovr_model * m )
{
	static cnovr_model * last_rendered_model = 0;
	cnovr_pose * pose;

	if( m->pose )
	{
		pose_to_matrix44( cnovrstate->mModel, m->pose );
		glUniformMatrix4fv( UNIFORMSLOT_MODEL, 1, 1, cnovrstate->mModel );
	}

	if( !m->bIsUploaded || m->nIBO < 0 ) return;
	//XXX Tricky: Don't lock model, so if we're loading while rendering, we don't hitch.
//	if( m != last_rendered_model )
	{
		//Try binding any textures.
		int i;
		int count = m->iTextures;
		cnovr_texture ** ts = m->pTextures;
		if( ts )
		{
			for( i = 0; i < count; i++ )
			{
				glActiveTextureCHEW( GL_TEXTURE0 + i );
				cnovr_texture * t = ts[i];
				CNOVRRender( t );
			}
		}

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->nIBO );

		count = m->iGeos;
		for( i = 0; i < count; i++ )
		{
			if( !m->pGeos[i]->bIsUploaded ) continue;
			glBindBuffer( GL_ARRAY_BUFFER, m->pGeos[i]->nVBO );
			//glVertexPointer( m->pGeos[i]->iStride, GL_FLOAT, m->pGeos[i]->iStride, 0);    // last param is offset, not ptr
			glVertexAttribPointer( i, m->pGeos[i]->iStride, GL_FLOAT, GL_FALSE, m->pGeos[i]->iStride*4, 0  );
		}
	}

//	glEnableClientState(GL_VERTEX_ARRAY);
	int mh = m->iRenderMesh;
	if( mh == -1 )
	{
		glDrawElements( m->nRenderType, m->iIndexCount, GL_UNSIGNED_INT, 0 );
	}
	else
	{
		int m1 = m->iMeshMarks[mh];
		int m2 = m->iMeshMarks[mh+1];
		//printf( "%d %d\n", m1, m2 );
		glDrawElements( m->nRenderType, m2-m1, GL_UNSIGNED_INT, ((int*)0) + m1 );
	}
}


cnovr_header cnovr_model_header = {
	(cnovrfn)CNOVRModelDelete, //Delete
	(cnovrfn)CNOVRModelRender, //Render
	TYPE_MODEL,
};


cnovr_model * CNOVRModelCreate( int initial_indices, int rendertype )
{
	cnovr_model * ret = malloc( sizeof( cnovr_model ) );
	memset( ret, 0, sizeof( cnovr_model ) );
	ret->base.header = &cnovr_model_header;
	ret->base.tccctx = TCCGetTag();

	ret->nIBO = -1;
	ret->iIndexCount = initial_indices;
	if( initial_indices < 1 ) initial_indices = 1;
	ret->pIndices = malloc( initial_indices * sizeof( GLuint ) );
	ret->nRenderType = rendertype;
	ret->iMeshMarks = malloc( sizeof( int ) * 2 );
	ret->iMeshMarks[0] = 0;
	ret->iMeshMarks[1] = 0;
	ret->nMeshes = 1;
	ret->sMeshMarks = 0;
	ret->iRenderMesh = -1;
	ret->sModifiers = 0;
	ret->iCollideMesh = -1;
	ret->iLoadOpaque1 = 0;
	ret->iLoadOpaque2 = 0;

	ret->pGeos = malloc( sizeof( cnovr_vbo * ) * 1 );
	ret->iGeos = 0;
	ret->pTextures = 0;
	ret->iTextures = 0;
	ret->geofile = 0;

	ret->bIsLoading = 0;
	ret->iLastVertMark = 0;
	ret->model_mutex = OGCreateMutex(); //XXX TODO USE ME!!!
	return ret;
}

void CNOVRModelSetNumVBOsWithStrides( cnovr_model * m, int vbos, ... )
{
	int i;
	va_list argp;
	va_start( argp, vbos );

	for( i = 0; i < m->iGeos; i++ )
	{
		CNOVRVBODelete( m->pGeos[i] );
	}
	m->pGeos = realloc( m->pGeos, sizeof( cnovr_vbo * ) * vbos );
	m->iGeos = vbos;

	for( i = 0; i < vbos; i++ )
	{
		int stride = va_arg(argp, int);
		m->pGeos[i] = CNOVRCreateVBO( stride, 0, 0, i );
	}
	va_end( argp );
}

void CNOVRModelSetNumVBOs( cnovr_model * m, int vbos )
{
	int i;
	for( i = 0; i < m->iGeos; i++ )
	{
		CNOVRVBODelete( m->pGeos[i] );
	}
	m->pGeos = realloc( m->pGeos, sizeof( cnovr_vbo * ) * vbos );
	m->iGeos = vbos;

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
	int i;
	for( i = 0; i < m->nMeshes; i++ )
	{
		if( m->sMeshMarks && m->sMeshMarks[i] ) free( m->sMeshMarks[i] );
	}
	m->sMeshMarks = realloc( m->sMeshMarks, sizeof( char * )*2 );
	m->iMeshMarks = realloc( m->iMeshMarks, sizeof( uint32_t )*2 );
	m->nMeshes = 1;
	m->iMeshMarks[0] = 0;
	m->sMeshMarks[1] = 0;
	m->iLastVertMark = 0;
}

void CNOVRDelinateGeometry( cnovr_model * m, const char * sectionname )
{
	if( m->iIndexCount == 0 )
	{
		m->nMeshes = 1;
	}
	else
	{
		m->nMeshes++;
	}
	m->iMeshMarks = realloc( m->iMeshMarks, sizeof( uint32_t ) * ( m->nMeshes + 2 ) );
	m->sMeshMarks = realloc( m->sMeshMarks, sizeof( char * ) * ( m->nMeshes + 2 ) );
	m->iMeshMarks[m->nMeshes-1] = m->iIndexCount;
	m->sMeshMarks[m->nMeshes-1] = strdup( sectionname );
}


void CNOVRModelTackIndex( cnovr_model * m, int nindices, ...)
{
	int iIndexCount = m->iIndexCount;
	m->pIndices = realloc( m->pIndices, (iIndexCount + nindices) * sizeof( m->pIndices[0] ) );
	uint32_t * pIndices = m->pIndices + iIndexCount;
	int i;
	va_list argp;
	va_start( argp, nindices );
	for( i = 0; i < nindices; i++ )
	{
		pIndices[i] = va_arg(argp, int);
	}
	va_end( argp );
	m->iIndexCount = m->iIndexCount + nindices;
}

void CNOVRModelTackIndexv( cnovr_model * m, int nindices, uint32_t * indices )
{
	int iIndexCount = m->iIndexCount;
	m->pIndices = realloc( m->pIndices, ( iIndexCount + nindices ) * sizeof( m->pIndices[0] ) );
	uint32_t * pIndices = m->pIndices + iIndexCount;
	int i;
	for( i = 0; i < nindices; i++ )
	{
		pIndices[i] = indices[i];
	}
	m->iIndexCount = m->iIndexCount + nindices;
}

void CNOVRModelClearMeshes( cnovr_model * m )
{
	OGLockMutex( m->model_mutex );
	CNOVRModelSetNumVBOsWithStrides( m, 0 );
	CNOVRModelSetNumIndices( m, 0 );
	CNOVRModelResetMarks( m );
	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelAppendCube( cnovr_model * m, cnovr_point3d size, cnovr_pose * poseofs_optional, cnovr_point4d * extradata )
{
	cnovr_pose * pose = poseofs_optional?poseofs_optional:&cnovr_pose_identity;

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
	if( m->iGeos != 4 )
	{
		CNOVRModelSetNumVBOsWithStrides( m, 4, 3, 3, 3, 4 );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
	}
	CNOVRDelinateGeometry( m, "cube" );

	int i;
	{
		uint32_t * indices = m->pIndices;
		for( i = 0; i < 36; i++ ) 
		{
			CNOVRModelTackIndex( m, 1, i + m->iLastVertMark );
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
				float staget[4];
				{
					float xyzin[3] = { (vkeys[j]&4)?1:-1, (vkeys[j]&2)?1:-1, (vkeys[j]&1)?1:-1 };
					mult3d( xyzin, xyzin, size );
					apply_pose_to_point( stage, pose, xyzin );
				}
				stage[3] = 1;
				CNOVRVBOTackv( m->pGeos[0], 3, stage );

				staget[0] = stage[tcaxis1] * 0.5 + 0.5;
				staget[1] = stage[tcaxis2] * 0.5 + 0.5;
				staget[2] = m->nMeshes;

				CNOVRVBOTackv( m->pGeos[1], 3, staget );

				stage[0] = (normaxis==0)?normplus:0;
				stage[1] = (normaxis==1)?normplus:0;
				stage[2] = (normaxis==2)?normplus:0;
				stage[3] = 0;
				CNOVRVBOTackv( m->pGeos[2], 3, stage );

				if( extradata )
					CNOVRVBOTackv( m->pGeos[3], 4, *extradata );
				else
					CNOVRVBOTackv( m->pGeos[3], 4, (float[4]){ 0, 0, 0, 0 } );
			}
		}
	}

	m->iLastVertMark += 36;

	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );
	CNOVRVBOTaint( m->pGeos[3] );
	CNOVRModelTaintIndices( m );

	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelAppendMesh( cnovr_model * m, int rows, int cols, int flipv, cnovr_point3d size, cnovr_pose * poseofs_optional, cnovr_point4d * extradata )
{
	//Copied from Spreadgine.
	int i;
	int x, y;
//	int c = w * h;
//	int v = (w+1)*(h+1);

	OGLockMutex( m->model_mutex );

	if( m->iGeos != 4 )
	{
		CNOVRModelSetNumVBOsWithStrides( m, 4, 3, 3, 3, 4  );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
	}

	CNOVRDelinateGeometry( m, "mesh" );

	{
		uint32_t * indices = m->pIndices;
		for( y = 0; y < rows; y++ )
		for( x = 0; x < cols; x++ )
		{
			int k = m->iLastVertMark;
			//printf( "MARK: %d\n", k );
			CNOVRModelTackIndex( m, 6, 
				k + x + y * (cols+1),
				k + (x+1) + y * (cols+1),
				k + (x+1) + (y+1) * (cols+1),
				k + (x) + (y) * (cols+1),
				k + (x+1) + (y+1) * (cols+1),
				k + (x) + (y+1) * (cols+1) );
		}
	}

	{
		float stage[4];
		float stagen[4];

		stagen[0] = 0;
		stagen[1] = 0;
		stagen[2] = -1;
		stagen[3] = m->nMeshes;

		for( y = 0; y <= rows; y++ )
		for( x = 0; x <= cols; x++ )
		{
			stage[0] = x/(float)rows;
			stage[1] = y/(float)cols;
			if( flipv ) stage[1] = 1.0 - stage[1];
			stage[2] = m->nMeshes;
			CNOVRVBOTackv( m->pGeos[1], 3, stage );	//TC
			stage[1] = y/(float)cols;
			stage[0] = (stage[0] - 0.5)*2.0 * size[0];
			stage[1] = (stage[1] - 0.5)*2.0 * size[1];
			stage[2] = size[2];
			if( poseofs_optional )
				apply_pose_to_point( stage, poseofs_optional, stage );
			CNOVRVBOTackv( m->pGeos[0], 3, stage );	//Pos
			CNOVRVBOTackv( m->pGeos[2], 3, stagen );

			if( extradata )
				CNOVRVBOTackv( m->pGeos[3], 4, *extradata );
			else
				CNOVRVBOTackv( m->pGeos[3], 4, (float[4]){ 0, 0, 0, 0 } );
		}
	}

	m->iLastVertMark += (rows+1)*(cols+1);

	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );
	CNOVRVBOTaint( m->pGeos[3] );
	CNOVRModelTaintIndices( m );

	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelApplyTextureFromFileAsync( cnovr_model * m, const char * sTextureFile )
{
	if( m->iTextures == 0 )
	{
		if( !m->pTextures ) m->pTextures = malloc( sizeof( cnovr_texture * ) );
		m->pTextures[0] = CNOVRTextureCreate( 1, 1, 4 );
		m->iTextures = 1;
	}
	CNOVRTextureLoadFileAsync( m->pTextures[0], sTextureFile );
}

void CNOVRModelSetNumTextures( cnovr_model * m, int textures )
{
	int i;
	for( i = textures; i < m->iTextures; i++ )
		CNOVRDelete( m->pTextures[i] );

	m->pTextures = realloc( m->pTextures, textures * sizeof( cnovr_texture * ) );

	for( i = m->iTextures; i < textures; i++ )
		m->pTextures[i] = CNOVRTextureCreate( 1, 1, 4 );

	m->iTextures = 1;
}

void CNOVRModelRenderWithPose( cnovr_model * m, cnovr_pose * pose )
{
	pose_to_matrix44( cnovrstate->mModel, pose );
	glUniformMatrix4fv( UNIFORMSLOT_MODEL, 1, 1, cnovrstate->mModel );
	m->base.header->Render( (cnovr_base*)m );
}

int  CNOVRModelCollide( cnovr_model * m, const cnovr_point3d start, const cnovr_vec3d direction, cnovr_collide_results * r, float dradius, float minimumt )
{
	int ret = -1;
	if( m->iGeos == 0 ) return -1;
	if( m->bIsLoading ) return -1;
	int iMeshNo = 0;
	//Iterate through all this.
	if( !m->pGeos[0] ) return -1;
	float * vpos = m->pGeos[0]->pVertices;
	int stride = m->pGeos[0]->iStride;
	int i;
	GLuint * indices = m->pIndices;
//	printf( "DIRECTION: %f %f %f\n", PFTHREE( direction ) );
	int startmesh = (m->iCollideMesh>=0)?m->iCollideMesh:0;
	int endmesh = (m->iCollideMesh>=0)?(m->iCollideMesh+1):m->nMeshes;
	for( i = startmesh; i < endmesh; i++ )
	{
		int meshStart = m->iMeshMarks[i];
		int meshEnd = (i == m->nMeshes-1 ) ? m->iIndexCount : m->iMeshMarks[i+1];
	//	printf( "%d   %d  %d\n", i, meshStart, meshEnd );
	//	printf( "%d %d\n", meshStart, meshEnd );
		int j;
		for( j = meshStart; j < meshEnd; j+=3 )
		{
			int i0 = indices[j+0];
			int i1 = indices[j+1];
			int i2 = indices[j+2];
			//i0..2 are the indices we will be verting.
			float * v0 = &vpos[i0*stride];
			float * v1 = &vpos[i1*stride];
			float * v2 = &vpos[i2*stride];

		//	if( i == 1 ) { printf( "%f %f %f  %f %f %f  %f %f %f\n", PFTHREE( v0 ), PFTHREE( v1 ), PFTHREE( v2) ); }

			float v10[3];
			float v21[3];
			float v02[3];
			float N[3];
			sub3d( v10, v1, v0 );
			sub3d( v21, v2, v1 );
			sub3d( v02, v0, v2 );

			//Current algorithm based on https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/ray-triangle-intersection-geometric-solution
			//XXX TODO: https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm looks faster than this.
			//XXX ALSO -> This is really cool: http://www.peroxide.dk/papers/collision/collision.pdf
			{
				float v01[3];
				sub3d( v01, v0, v1 ); //!?!?!
				cross3d( N, v21, v01 );
			}
			float Ax2 = magnitude3d( N );
			scale3d( N, N, 1./Ax2 );

			float D = -dot3d(N, v0);

			//Distance from contact location to underlying geometry
			float thissndist = (dot3d( N, start ) + D - dradius);

			float t = -thissndist / dot3d( N, direction ); 

			float Phit[3];
			scale3d( Phit, direction, t );
			add3d( Phit, Phit, start );
			float C0[3];
			float C1[3];
			float C2[3];

			{
				float C0tmp[3];
				float C1tmp[3];
				float C2tmp[3];
				sub3d( C0tmp, Phit, v0 );
				sub3d( C1tmp, Phit, v1 );
				sub3d( C2tmp, Phit, v2 );
				cross3d( C0, v10, C0tmp );
				cross3d( C1, v21, C1tmp );
				cross3d( C2, v02, C2tmp );
			}

			//C1-2 are busted (?) here. Their magnitude is weird.  Almost like it's squared or something?
			float t0 = dot3d( N, C0 );
			float t1 = dot3d( N, C1 );
			float t2 = dot3d( N, C2 );
	//		printf( "TRIS: %f %f %f   %f %f %f > %f/%f > %f/%f\n", t0, t1, t2, PFTHREE( N ), t, thissndist, r->t, r->sndist );

			if( t0 < -0.0001 || t1 < -.0001 || t2 < -.0001 ||
				t0 != t0 || t1 != t1 || t2 != t2  || /* Make sure we don't have a NaN */
				t < minimumt )
			//if( 1 )
			{
				if( dradius <= 0 )
					continue;

				cnovr_point3d geonormbase;

				//We did not hit the triangle, itself.
				float pv0[3];
				float pv1[3];
				float pv2[3];
				float drsq = dradius*dradius;
				sub3d( pv0, start, v0 );
				sub3d( pv1, start, v1 );
				sub3d( pv2, start, v2 );

				int didhit = 0;
				t = r->t;

				//Check vetices.
				cnovr_point3d ptsolutions;
				if( 1 ){
					cnovr_point3d pC_C = { dot3d( pv0, pv0 )-drsq, dot3d( pv1, pv1 )-drsq, dot3d( pv2, pv2 )-drsq };
					cnovr_point3d pC_B = { 2*dot3d( direction, pv0 ), 2*dot3d( direction, pv1 ), 2*dot3d( direction, pv2 ) };
					float pCxA = dot3d( direction, direction );

					cnovr_point3d discriminants;
					cnovr_point3d tmp, tmp2;
					mult3d( tmp, pC_B, pC_B ); //B^2
					scale3d( tmp2, pC_C, -4*pCxA ); //-4AC
					add3d( discriminants, tmp, tmp2 );
					pCxA *= 2;
					ptsolutions[0] = (-pC_B[0] - sqrt( discriminants[0] ))/pCxA;
					ptsolutions[1] = (-pC_B[1] - sqrt( discriminants[1] ))/pCxA;
					ptsolutions[2] = (-pC_B[2] - sqrt( discriminants[2] ))/pCxA;

					//printf( "%f    %f %f %f    %f %f %f   %f %f %f  %f %f %f\n", pCxA, PFTHREE( pC_B ), PFTHREE( pC_C ), PFTHREE( discriminants ), PFTHREE( ptsolutions ) );
					if( !( ptsolutions[0] != ptsolutions[0] || ptsolutions[0] > t || ptsolutions[0] < minimumt ) ) { didhit = 1; t = ptsolutions[0]; copy3d( geonormbase, v0 ); }
					if( !( ptsolutions[1] != ptsolutions[1] || ptsolutions[1] > t || ptsolutions[1] < minimumt ) ) { didhit = 1; t = ptsolutions[1]; copy3d( geonormbase, v1 ); }
					if( !( ptsolutions[2] != ptsolutions[2] || ptsolutions[2] > t || ptsolutions[2] < minimumt ) ) { didhit = 1; t = ptsolutions[2]; copy3d( geonormbase, v2 ); }
				}

				//Check edges.
				cnovr_point3d edgesolutions = { 0./0., 0./0., 0./0. };
				{
					//In http://www.peroxide.dk/papers/collision/collision.pdf
					//"edge" refers to v10, v21, v02
					//"baseToVertex" refers to -pv_<<<<<
					//"velocity" refers to direction
					cnovr_point3d tmp1, tmp2;
					float tmp;
					cnovr_point3d edgesquared;
					cnovr_point3d A,B,C;
					edgesquared[0] = dot3d( v10, v10 );
					edgesquared[1] = dot3d( v21, v21 );
					edgesquared[2] = dot3d( v02, v02 );
					tmp = -dot3d( direction, direction );
					scale3d( tmp1, edgesquared, tmp );
					tmp2[0] = dot3d( v10, direction );
					tmp2[1] = dot3d( v21, direction );
					tmp2[2] = dot3d( v02, direction );
					mult3d( tmp2, tmp2, tmp2 ); //Dot squared  ?!?!?!?!
					add3d( A, tmp1, tmp2 );

					tmp1[0] = -2*dot3d( direction, pv0 )*edgesquared[0];
					tmp1[1] = -2*dot3d( direction, pv1 )*edgesquared[1];
					tmp1[2] = -2*dot3d( direction, pv2 )*edgesquared[2];
					tmp2[0] =  2*dot3d(v10,direction)*dot3d(v10,pv0);
					tmp2[1] =  2*dot3d(v21,direction)*dot3d(v21,pv1);
					tmp2[2] =  2*dot3d(v02,direction)*dot3d(v02,pv2);
					add3d( B, tmp1, tmp2 );

					tmp1[0] = edgesquared[0]*(drsq-dot3d(pv0, pv0));
					tmp1[1] = edgesquared[1]*(drsq-dot3d(pv1, pv1));
					tmp1[2] = edgesquared[2]*(drsq-dot3d(pv2, pv2));
					tmp2[0] = -dot3d(v10,pv0);
					tmp2[1] = -dot3d(v21,pv1);
					tmp2[2] = -dot3d(v02,pv2);
					mult3d( tmp2, tmp2, tmp2 );
					add3d( C, tmp1, tmp2 );

					//b^2-4ac
					cnovr_point3d x1;
					x1[0] = (-B[0] + sqrt( B[0]*B[0] - 4 * A[0] * C[0] )) / ( 2 * A[0] );
					x1[1] = (-B[1] + sqrt( B[1]*B[1] - 4 * A[1] * C[1] )) / ( 2 * A[1] );
					x1[2] = (-B[2] + sqrt( B[2]*B[2] - 4 * A[2] * C[2] )) / ( 2 * A[2] );

					cnovr_point3d f0;
					f0[0] = ( dot3d( v10, direction ) * x1[0] + dot3d( v10, pv0 ) ) / edgesquared[0];
					f0[1] = ( dot3d( v21, direction ) * x1[1] + dot3d( v21, pv1 ) ) / edgesquared[1];
					f0[2] = ( dot3d( v02, direction ) * x1[2] + dot3d( v02, pv2 ) ) / edgesquared[2];

					if( f0[0] >= 0 && f0[0] <= 1 ) edgesolutions[0] = x1[0];
					if( f0[1] >= 0 && f0[1] <= 1 ) edgesolutions[1] = x1[1];
					if( f0[2] >= 0 && f0[2] <= 1 ) edgesolutions[2] = x1[2];
					//printf( "%f %f %f   %f %f %f   %f %f %f     %f %f %f   %f %f %f   %f %f %f  %d %d %f\n", PFTHREE( A ), PFTHREE( B ), PFTHREE( C ), PFTHREE( x1 ), PFTHREE( f0 ), PFTHREE( edgesolutions ), edgesolutions[1] != edgesolutions[1], edgesolutions[1] > t, t  );
					if( !( edgesolutions[0] != edgesolutions[0] || edgesolutions[0] > t || edgesolutions[0] < minimumt ) ) { didhit = 1; t = edgesolutions[0]; scale3d( geonormbase, v10, f0[0] ); add3d (geonormbase, geonormbase, v0 ); }
					if( !( edgesolutions[1] != edgesolutions[1] || edgesolutions[1] > t || edgesolutions[1] < minimumt ) ) { didhit = 1; t = edgesolutions[1]; scale3d( geonormbase, v21, f0[1] ); add3d (geonormbase, geonormbase, v1 ); }
					if( !( edgesolutions[2] != edgesolutions[2] || edgesolutions[2] > t || edgesolutions[2] < minimumt ) ) { didhit = 1; t = edgesolutions[2]; scale3d( geonormbase, v02, f0[2] ); add3d (geonormbase, geonormbase, v2 ); }
				}
				if( !didhit ) continue;
				//cnovr_point3d hitpos;
				scale3d( Phit, direction, t );
				add3d( Phit, Phit, start );
				sub3d( r->geonorm, Phit, geonormbase );
				r->sndist = magnitude3d( r->geonorm ) - dradius; //checks out OK
				normalize3d( r->geonorm, r->geonorm );
				//Now that we've calculated the norm, we can compute what the nominal penetration would be.

				if( t < 0 )
				{
					//XXX This is rough.  I'm not sure how to solve it, but it is possible to
					//overshoot the edge of a cylinder/sphere and get a value higher than the maximum penetration.
					//This can happen at extreme glancing angles.  Right now, we do not use triangle information to fix this.
					//Perhaps we should.  
					cnovr_point3d zerohit;
					sub3d( zerohit, start, geonormbase );
					r->sndist = magnitude3d( zerohit ) - dradius;
					//float outsndist = (dot3d( N, Phit ) + D - dradius); //Use triangle information to fix.
					//if( r->sndist > outsndist ) r->sndist = outsndist; 
					if( r->sndist > 0 ) r->sndist = 0;
					//printf( "K %f %f (%f)\n", r->sndist, t, magnitude3d( zerohit ) );
				}
				else
				{
				//	printf( "L %f\n", r->sndist );
				}
				//printf( "B %f %f\n", t, r->sndist );
			}
			else
			{
				//continue;
				//We got a proper triangle hit.
				if( t < r->t && t >= minimumt)
				{
					float outsndist = (t<0)?thissndist:(dot3d( N, Phit ) + D - dradius);

					//printf( "A %f", outsndist );
					r->sndist = outsndist;
					copy3d( r->geonorm, N );
					//if( r->sndist < 0 ) printf( "T: %f\n", r->sndist );
				}
			}

			//Else: We have a hit.  This doesn't happen for all that many polys, so time isn't as critical here.
			//The following code is triggered for triangles or points,
			if( t < r->t && t >= minimumt )
			{
				r->t = t;
				r->whichmesh = i;
				r->whichvert = j;
				copy3d( r->collidepos, Phit );

				if( m->iGeos > 1 )
				{
					t0/=Ax2;
					t1/=Ax2;
					t2/=Ax2;

					//{ t0, t1, t2 } are the barycentric coordinates. 
					int stride1 = m->pGeos[1]->iStride;
					float * vd1 = m->pGeos[1]->pVertices;
					float * tx0 = vd1 + i0 * stride1;
					float * tx1 = vd1 + i1 * stride1;
					float * tx2 = vd1 + i2 * stride1;
					float * txo = r->collidevs;
					int j;
					for( j = 0; j < stride1; j++ )
					{
						txo[j] = tx0[j] * t1 + tx1[j] * t2 + tx2[j] * t0;
					}
					for( ; j < sizeof(r->collidevs) / sizeof(r->collidevs[0]); j++ )
					{
						txo[j] = 0;
					}

					//Also get normal?
					stride1 = m->pGeos[2]->iStride;
					vd1 = m->pGeos[2]->pVertices;
					tx0 = vd1 + i0 * stride1;
					tx1 = vd1 + i1 * stride1;
					tx2 = vd1 + i2 * stride1;
					txo = r->collidens;
					for( j = 0; j < stride1; j++ )
					{
						txo[j] = tx0[j] * t1 + tx1[j] * t2 + tx2[j] * t0;
					}
					//printf( "%f %f %f %d  %f %f %f\n", PFTHREE( tx0 ), stride1, PFTHREE( txo ) );
					for( ; j < sizeof(r->collidens) / sizeof(r->collidens[0]); j++ )
					{
						txo[j] = 0;
					}
//					printf( "CONTACT : %f\n", t );
					//printf( "%f * (%f %f %f)  %f * (%f %f %f)  %f * (%f %f %f)\n", t1, PFTHREE( tx0 ), t2, PFTHREE( tx1 ), t0, PFTHREE( tx2 ) );
				}
				ret = i;
			}
		}
	}
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//Model Loaders

#define OBJBUFFERSIZE 256

///
/// OBJ File Loader (From Spreadgine)
///

#define TBUFFERSIZE 256
#define VBUFFERSIZE 524288

struct TempObject
{
	int     CVertCount;
	float * CVerts;
	int     CNormalCount;
	float * CNormals;
	int     CTexCount;
	float * CTexs;
};

void CNOVRModelLoadFromFileAsyncCallback( void * vm, void * dump );

//int rttt;
//#define RBlvpcmp(x,y) ( rttt = memcmp( x, y, sizeof(cnovr_point3d) ), printf( "%f %f %f // %f %f %f %d\n", PFTHREE( x ), PFTHREE( y ), rttt ), rttt )
#define RBlvpcmp(x,y) ( memcmp( x, y, sizeof(cnovr_point3d) ) )
#define RBlvpcpy(x,y,z) { copy3d( x, y ); }
#define RBlvpdel(x,y) {}
// free( x );

typedef cnovr_point3d * cnovr_point3dptr;
CNRBTREETEMPLATE( cnovr_point3d, int, RBlvpcmp, RBlvpcpy, RBlvpdel );
CNRBTREETEMPLATE( int64_t, rbset_null_t, RBptrcmp, RBptrcpy, RBnullop );

typedef cnrbtree_cnovr_point3dint linevertexpair;
typedef cnrbtree_int64_trbset_null_t indexpairset;

static void CNOVRModelLoadOBJ( cnovr_model * m, const char * filename, const char * modifiers )
{
	int filelen;
	char * file = CNOVRFileToString( filename, &filelen );

	if( m->iLoadOpaque1 != filelen )
	{
		m->iLoadOpaque2 = 0;
		m->iLoadOpaque1 = filelen;
	}
	else if( m->iLoadOpaque2 < 2 )
	{
		m->iLoadOpaque2++;
		CNOVRJobTack( cnovrQAsync, CNOVRModelLoadFromFileAsyncCallback, m, 0, 1 );
		free( file );
		return;
	}
	m->iLoadOpaque2 = 0;
	char ** splits = CNOVRSplitStrings( file, "\n", "\r", 1, 0 );
	free( file );

	int lineify = 0;
	int flipv = 1;
	if( modifiers )
	{
		if( strstr( modifiers, "lineify" ) ) lineify = 1;
		if( strstr( modifiers, "noflipv" ) ) flipv = 0;
	}

	struct TempObject t;
	memset( &t, 0, sizeof( t ) );
	if( m->iGeos != 3 )
	{
		CNOVRModelSetNumVBOsWithStrides( m, 3, 3, 4, 3 );
		CNOVRModelSetNumIndices( m, 0 );
		CNOVRModelResetMarks( m );
	}
	CNOVRModelSetNumIndices( m, 0 );

	int indices = 0;
	char * line;
	int lineno;
	int nObjNo = 0;
	
	linevertexpair * lvps = 0;
	indexpairset   * lvpsi = 0;
	if( lineify )
	{
		lvps = cnrbtree_cnovr_point3dint_create();
		lvpsi = cnrbtree_int64_trbset_null_t_create();
	}
	
	for( lineno = 0; (line = splits[lineno]) ; lineno++ )
	{
		int linelen = strlen( line );
		if( linelen < 2 ) continue;
		if( tolower( line[0] ) == 'v' )
		{
			if( tolower( line[1] ) == 'n' )
			{
				t.CNormals = realloc( t.CNormals, ( t.CNormalCount + 1 ) * 3 * sizeof( float ) );
				int r = sscanf( line + 3, "%f %f %f", 
					&t.CNormals[0 + t.CNormalCount * 3], 
					&t.CNormals[1 + t.CNormalCount * 3], 
					&t.CNormals[2 + t.CNormalCount * 3] );
				//t.CNormals[3 + t.CNormalCount * 3] = 0;
				if( r == 3 )
					t.CNormalCount++;
			}
			else if( tolower( line[1] ) == 't' )
			{
				t.CTexs = realloc( t.CTexs, ( t.CTexCount + 1 ) * 4 * sizeof( float ) );
				t.CTexs[1 + t.CTexCount * 4] = 0;
				t.CTexs[2 + t.CTexCount * 4] = 0;
				t.CTexs[3 + t.CTexCount * 4] = nObjNo;
				int r = sscanf( line + 3, "%f %f %f", 
					&t.CTexs[0 + t.CTexCount * 4], 
					&t.CTexs[1 + t.CTexCount * 4], 
					&t.CTexs[2 + t.CTexCount * 4] );

				if( flipv )
					t.CTexs[1 + t.CTexCount * 4] = 1. - t.CTexs[1 + t.CTexCount * 4];
				if( r == 3 || r == 2 )
					t.CTexCount++;
				else
				{
					CNOVRAlert( m->base.tccctx, 1, "Error: Invalid Tex Coords (%d) (%s)\n", r, line + 3 );
				}
			}
			else
			{
				t.CVerts = realloc( t.CVerts, ( t.CVertCount + 1 ) * 3 * sizeof( float ) );
				t.CVerts[2 + t.CVertCount * 3] = 0;
				int r = sscanf( line + 2, "%f %f %f", 
					&t.CVerts[0 + t.CVertCount * 3], 
					&t.CVerts[1 + t.CVertCount * 3], 
					&t.CVerts[2 + t.CVertCount * 3] );
				if( r == 3 || r == 2 )
					t.CVertCount++;
				else
				{
					fprintf( stderr, "Error: Invalid Vertex\n" );
				}
			}
		}
		else if( tolower( line[0] ) == 'f' )
		{
			char buffer[4][TBUFFERSIZE];
			int p = 0;
			int r = sscanf( line + 1, "%30s %30s %30s %30s", 
				buffer[0], buffer[1], buffer[2], buffer[3] );

			if( r != 3 && r != 4 )
			{
				//Invalid line - continue.
				continue;
			}

			//Whatever... they're all populated with something now.

			char buffer2[4 /*max # of elements*/][4][TBUFFERSIZE];
			for( p = 0; p < r; p++ )
			{
				int mark = 0, markb = 0;
				int i;
				int sl = strlen( buffer[p] );
				for( i = 0; i < sl; i++ )
				{
					if( buffer[p][i] == '/' )
					{
						buffer2[p][mark][markb] = 0;
						mark++;
						if( mark >= r ) break;
						markb = 0;
					}
					else
						buffer2[p][mark][markb++] = buffer[p][i];
				}
				buffer2[p][mark][markb] = 0;
				for( i = mark+1; i < r; i++ )
				{
					buffer2[p][i][0] = '0';
					buffer2[p][i][1] = 0;
				}
			}
			

			if( lineify )
			{
				for( p = 0; p < r; p++ )
				{
					int iao1o = p;
					int iao2o = ( p < r-1 )?(p+1):(p-(r-1));
					int VNumber1 = atoi( buffer2[iao1o][0] ) - 1;
					int VNumber2 = atoi( buffer2[iao2o][0] ) - 1;
					int TNumber1 = atoi( buffer2[iao1o][1] ) - 1;
					int TNumber2 = atoi( buffer2[iao2o][1] ) - 1;
					int NNumber1 = atoi( buffer2[iao1o][2] ) - 1;
					int NNumber2 = atoi( buffer2[iao2o][2] ) - 1;

					cnovr_point3d va;
					cnovr_point3d vb;
					copy3d( va, &t.CVerts[VNumber1*3] );
					copy3d( vb, &t.CVerts[VNumber2*3] );
					int i1;
					int i2;
					if( !RBHAS( lvps, va ) )
					{
						i1 = RBA( lvps, va ) = m->pGeos[0]->iVertexCount;
						if( VNumber1 < t.CVertCount && VNumber1 >= 0 )
							CNOVRVBOTackv( m->pGeos[0], 3, &t.CVerts[VNumber1*3] );
						else
							CNOVRVBOTack( m->pGeos[0], 3, 0, 0, 0 );

						if( TNumber1 < t.CTexCount && TNumber1 >= 0 )
							CNOVRVBOTackv( m->pGeos[1], 4, &t.CTexs[TNumber1*4] );
						else
							CNOVRVBOTack( m->pGeos[1], 4, 0, 0, 0, nObjNo );
						if( NNumber1 < t.CNormalCount && NNumber1 >= 0 )
							CNOVRVBOTackv( m->pGeos[2], 3, &t.CNormals[NNumber1*3] );
						else
							CNOVRVBOTack( m->pGeos[2], 3, 0, 0, 0 );
					}
					else
					{
						i1 = RBA( lvps, va );
					}
					if( !RBHAS( lvps, vb ) )
					{
						i2 = RBA( lvps, vb ) = m->pGeos[0]->iVertexCount;
						if( VNumber2 < t.CVertCount && VNumber2 >= 0 )
							CNOVRVBOTackv( m->pGeos[0], 3, &t.CVerts[VNumber2*3] );
						else
							CNOVRVBOTack( m->pGeos[0], 3, 0, 0, 0 );

						if( TNumber2 < t.CTexCount && TNumber2 >= 0 )
							CNOVRVBOTackv( m->pGeos[1], 4, &t.CTexs[TNumber2*4] );
						else
							CNOVRVBOTack( m->pGeos[1], 4, 0, 0, 0, nObjNo );
						if( NNumber2 < t.CNormalCount && NNumber2 >= 0 )
							CNOVRVBOTackv( m->pGeos[2], 3, &t.CNormals[NNumber2*3] );
						else
							CNOVRVBOTack( m->pGeos[2], 3, 0, 0, 0 );
					}
					else
					{
						i2 = RBA( lvps, vb );
					}

					//printf( "I: %d (%f %f %f) %d (%f %f %f)\n", i1, PFTHREE( va ), i2, PFTHREE( vb ) );
					
					int64_t paria = i1 | (((int64_t)i2)<<32);
					int64_t parib = i2 | (((int64_t)i1)<<32);
					//printf( "%llx %llx  %p %p\n", (uint64_t)paria, (uint64_t)parib, RBHAS( lvpsi, paria ), RBHAS( lvpsi, parib ) );
					if( !RBHAS( lvpsi, paria ) && !RBHAS( lvpsi, parib ) )
					{
						//printf( "ACCESS: %p {%d %d}\n", 
							lvpsi->access( lvpsi, paria )
						//, i1, i2 )
						;
						
						//Now, examine if i1, i2 is already in the set.
						CNOVRModelTackIndex( m, 2, i1, i2 );
						indices+=2;
					}
				}
			}
			else
			{
				for( p = 0; p < r; p++ )
				{
					int VNumber = atoi( buffer2[p][0] ) - 1;
					int TNumber = atoi( buffer2[p][1] ) - 1;
					int NNumber = atoi( buffer2[p][2] ) - 1;

					CNOVRModelTackIndex( m, 1, indices++ );
					if( VNumber < t.CVertCount && VNumber >= 0 )
						CNOVRVBOTackv( m->pGeos[0], 3, &t.CVerts[VNumber*3] );
					else
						CNOVRVBOTack( m->pGeos[0], 3, 0, 0, 0 );

					if( TNumber < t.CTexCount && TNumber >= 0 )
						CNOVRVBOTackv( m->pGeos[1], 4, &t.CTexs[TNumber*4] );
					else
						CNOVRVBOTack( m->pGeos[1], 4, 0, 0, 0, nObjNo );

					if( NNumber < t.CNormalCount && NNumber >= 0 )
						CNOVRVBOTackv( m->pGeos[2], 3, &t.CNormals[NNumber*3] );
					else
						CNOVRVBOTack( m->pGeos[2], 3, 0, 0, 0 );
				}

			}
		}
		else if( tolower( line[0] ) == 'o' )
		{
			const char * marker = ( strlen( line ) > 2 )?( line + 2 ):0;
			nObjNo++;
			CNOVRDelinateGeometry( m, marker );
		}
		else if( strncmp( line, "usemtl", 6 ) == 0 )
		{
			//Not implemented.
		}
		else if( strncmp( line, "mtllib", 6 ) == 0 )
		{
			//Not implemented.
		}
		else if( tolower( line[0] ) == 's' )
		{
			//Not implemented.
		}
	}

	printf( "LOADED MODEL {%s} %d INDICES\n", filename, indices );

	if( lvps )
	{
		cnrbtree_cnovr_point3dint_destroy( lvps );
		cnrbtree_int64_trbset_null_t_destroy( lvpsi );
	}
	if( t.CVerts ) free( t.CVerts ); 
	if( t.CTexs ) free( t.CTexs ); 
	if( t.CNormals ) free( t.CNormals ); 
	free( splits );

	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );
	CNOVRModelTaintIndices( m );
	return;
}



static void CNOVRModelLoadRenderModel( cnovr_model * m, char * pchRenderModelNameIn, const char * modifiers )
{
	RenderModel_t * pModel = NULL;

	int lineify = 0;
	int flipv = 1;
	if( modifiers )
	{
		if( strstr( modifiers, "lineify" ) ) lineify = 1;
		if( strstr( modifiers, "noflipv" ) ) flipv = 0;
	}

	if( !cnovrstate->oRenderModels )
	{
		CNOVRAlert( m->base.tccctx, 1, "Attempted to run a rendermodel when openvr was not running\n" );
		return;
	}

	int rnnamelen = pchRenderModelNameIn?strlen( pchRenderModelNameIn ):0;
	if( !rnnamelen )
	{
		CNOVRAlert( m->base.tccctx, 1, "Unable to load render model %s (zero-length)\n", pchRenderModelNameIn );
		return;
	}
	if( strcasecmp( pchRenderModelNameIn + rnnamelen - (sizeof( ".rendermodel" )-1), ".rendermodel" ) == 0 )
	{
		rnnamelen -=( sizeof( ".rendermodel" )-1 );
	}
	char * pchRenderModelName = alloca( rnnamelen + 1 );
	memcpy( pchRenderModelName, pchRenderModelNameIn, rnnamelen + 1 );
	pchRenderModelName[rnnamelen] = 0;
	
	while( cnovrstate->oRenderModels->LoadRenderModel_Async( pchRenderModelName, &pModel ) == EVRRenderModelError_VRRenderModelError_Loading)
	{
		OGUSleep( 100 );
	}

	if( cnovrstate->oRenderModels->LoadRenderModel_Async( pchRenderModelName, &pModel ) || pModel == NULL )
	{
		CNOVRAlert( m->base.tccctx, 1, "Unable to load render model %s (ASync begin)\n", pchRenderModelName );
		return;
	}	
	RenderModel_TextureMap_t *pTexture = NULL;

	while( cnovrstate->oRenderModels->LoadTexture_Async( pModel->diffuseTextureId, &pTexture ) == EVRRenderModelError_VRRenderModelError_Loading )
	{
		OGUSleep( 100 );
	}

	if( cnovrstate->oRenderModels->LoadTexture_Async(pModel->diffuseTextureId, &pTexture) || pTexture == NULL )
	{
		CNOVRAlert( m->base.tccctx, 1, "Unable to load render model %s (Texture fail)\n", pchRenderModelName );
		cnovrstate->oRenderModels->FreeRenderModel( pModel );
		return;
	}

	CNOVRAlert( m->base.tccctx, 5, "Loading rendermodel %s\n", pchRenderModelName );

	if( m->iGeos != 3 )
	{
		CNOVRModelSetNumVBOsWithStrides( m, 3, 3, 2, 3 );
		CNOVRModelResetMarks( m );
	}
	CNOVRModelSetNumIndices( m, 0 );
	CNOVRDelinateGeometry( m, pchRenderModelName );
	int i;
	if( lineify )
	{
		linevertexpair * lvps = 0;
		indexpairset   * lvpsi = 0;
		lvps = cnrbtree_cnovr_point3dint_create();
		lvpsi = cnrbtree_int64_trbset_null_t_create();

		for( i = 0; i < pModel->unTriangleCount*3; i++ )
		{
			int iao1o = i;
			int iao2o = ((i%3)!=2)?(i+1):(i-2);
			int VNumber1 = pModel->rIndexData[iao1o];
			int VNumber2 = pModel->rIndexData[iao2o];

			RenderModel_Vertex_t * v1 = pModel->rVertexData + VNumber1;
			RenderModel_Vertex_t * v2 = pModel->rVertexData + VNumber2;

			cnovr_point3d va;
			cnovr_point3d vb;
			copy3d( va, v1->vPosition.v );
			copy3d( vb, v2->vPosition.v );
			int i1;
			int i2;
			if( !RBHAS( lvps, va ) )
			{
				i1 = RBA( lvps, va ) = m->pGeos[0]->iVertexCount;
				if( VNumber1 < pModel->unVertexCount && VNumber1 >= 0 )
				{
					CNOVRVBOTackv( m->pGeos[0], 3, v1->vPosition.v );
					CNOVRVBOTackv( m->pGeos[1], 2, v1->rfTextureCoord );
					CNOVRVBOTackv( m->pGeos[2], 3, v1->vNormal.v );
				}
				else
				{
					CNOVRVBOTack( m->pGeos[0], 3, 0, 0, 0 );
					CNOVRVBOTack( m->pGeos[1], 2, 0, 0 );
					CNOVRVBOTack( m->pGeos[2], 3, 0, 0, 0 );
				}
			}
			else
			{
				i1 = RBA( lvps, va );
			}
			if( !RBHAS( lvps, vb ) )
			{
				i2 = RBA( lvps, vb ) = m->pGeos[0]->iVertexCount;
				if( VNumber2 < pModel->unVertexCount && VNumber2 >= 0 )
				{
					CNOVRVBOTackv( m->pGeos[0], 3, v2->vPosition.v );
					CNOVRVBOTackv( m->pGeos[1], 2, v2->rfTextureCoord );
					CNOVRVBOTackv( m->pGeos[2], 3, v2->vNormal.v );
				}
				else
				{
					CNOVRVBOTack( m->pGeos[0], 3, 0, 0, 0 );
					CNOVRVBOTack( m->pGeos[1], 2, 0, 0 );
					CNOVRVBOTack( m->pGeos[2], 3, 0, 0, 0 );
				}
			}
			else
			{
				i2 = RBA( lvps, vb );
			}
			int64_t paria = i1 | (((int64_t)i2)<<32);
			int64_t parib = i2 | (((int64_t)i1)<<32);
			//printf( "%llx %llx  %p %p\n", (uint64_t)paria, (uint64_t)parib, RBHAS( lvpsi, paria ), RBHAS( lvpsi, parib ) );
			if( !RBHAS( lvpsi, paria ) && !RBHAS( lvpsi, parib ) )
			{
				lvpsi->access( lvpsi, paria );
				
				//Now, examine if i1, i2 is already in the set.
				CNOVRModelTackIndex( m, 2, i1, i2 );
			}
		}

		cnrbtree_cnovr_point3dint_destroy( lvps );
		cnrbtree_int64_trbset_null_t_destroy( lvpsi );

	}
	else
	{
		//Regular triangles
		for( i = 0; i < pModel->unVertexCount; i++ )
		{
			RenderModel_Vertex_t * v = pModel->rVertexData + i;
			CNOVRVBOTackv( m->pGeos[0], 3, v->vPosition.v );
			if( !flipv ) v->rfTextureCoord[1] = 1-v->rfTextureCoord[1];
			CNOVRVBOTackv( m->pGeos[1], 2, v->rfTextureCoord );
			CNOVRVBOTackv( m->pGeos[2], 3, v->vNormal.v );
		}
		for( i = 0; i < pModel->unTriangleCount; i++ )
		{
			CNOVRModelTackIndex( m, 3, pModel->rIndexData[i*3+0], pModel->rIndexData[i*3+1], pModel->rIndexData[i*3+2] );
		}
	}

	if( m->iTextures == 0 )
	{
		m->pTextures = realloc( m->pTextures, sizeof( cnovr_texture * ) * 1 );
		m->pTextures[0] = CNOVRTextureCreate( 1, 1, 4 );
		m->iTextures = 1;
	}
	uint8_t * copyTexture = malloc( pTexture->unWidth * pTexture->unHeight * 4 );
	memcpy( copyTexture, pTexture->rubTextureMapData, pTexture->unWidth * pTexture->unHeight * 4 );
	CNOVRTextureLoadDataAsync( m->pTextures[0], pTexture->unWidth, pTexture->unHeight, 4, 0, copyTexture );

	CNOVRVBOTaint( m->pGeos[0] );
	CNOVRVBOTaint( m->pGeos[1] );
	CNOVRVBOTaint( m->pGeos[2] );
	CNOVRModelTaintIndices( m );
	cnovrstate->oRenderModels->FreeRenderModel( pModel );
	cnovrstate->oRenderModels->FreeTexture( pTexture );
}


void CNOVRModelLoadFromFileAsyncCallback( void * vm, void * dump )
{
	cnovr_model * m = (cnovr_model*) vm;
	OGLockMutex( m->model_mutex );
	char * filename = m->geofile;
	int slen = strlen( filename );
	m->bIsLoading = 1;
	if( CNOVRStringCompareEndingCase( filename, ".obj" ) == 0 )
	{
		CNOVRModelLoadOBJ( m, filename, m->sModifiers );
	}
	else if( CNOVRStringCompareEndingCase( filename, ".rendermodel" ) == 0 )
	{
		CNOVRModelLoadRenderModel( m, filename, m->sModifiers );
	}
	else
	{
		CNOVRAlert( m->base.tccctx, 1, "Error: Could not open model file: \"%s\".\n", filename );
	}
	m->bIsLoading = 0;
	OGUnlockMutex( m->model_mutex );
}

void CNOVRModelLoadFromFileAsync( cnovr_model * m, const char * filename )
{
	OGLockMutex( m->model_mutex );
	if( m->geofile ) free( m->geofile );
	char * colon = strchr( filename, ':' );
	if( colon )
	{
		m->sModifiers = strdup( colon + 1 );
		char tmpfn[CNOVR_MAX_PATH];
		memcpy( tmpfn, filename, colon - filename );
		tmpfn[colon-filename] = 0;
		const char * gfile = CNOVRFileSearch( tmpfn );
		m->geofile = strdup( (gfile&&gfile[0])?gfile:tmpfn ); //In case it's a render model.		
	}	
	else
	{
		const char * gfile = CNOVRFileSearch( filename );
		m->geofile = strdup( (gfile&&gfile[0])?gfile:filename ); //In case it's a render model.
	}
	CNOVRJobTack( cnovrQAsync, CNOVRModelLoadFromFileAsyncCallback, m, 0, 1 );
	CNOVRFileTimeAddWatch( m->geofile, CNOVRModelLoadFromFileAsyncCallback, m, 0 );
	OGUnlockMutex( m->model_mutex );
}

/////////////////////////////////////////////////////////////////////////////////////

void CNOVRNodeDelete( void * ths, void * opaque )
{
	int i;
	CNOVRListDeleteTag( ths );

	cnovr_simple_node * node = (cnovr_simple_node *)ths;
	sb_free( node->objects );

#if 0
	//I think the behavior we desire is to not auto delete.
	cnovr_simple_node * n = (cnovr_simple_node*)ths;
	cnovr_base ** objects = (n->objects);
	int count = sb_count( objects );
	for( i = 0; i < count; i++ )
	{
		cnovr_header * o = objects[i]->header;
		if( o->Delete ) o->Delete( objects[i] );
	}
#endif
	CNOVRFreeLater( ths );
}

void CNOVRNodeRender( void * ths )
{
	int i;
	cnovr_simple_node * n = (cnovr_simple_node*)ths;
	cnovr_base ** objects = (n->objects);
	for( i = 0; i < sb_count( objects ); i++ )
	{
		cnovr_base * b = objects[i];
		cnovr_header * o = b->header;
		if( o->Type == TYPE_MODEL )
		{
			CNOVRModelRenderWithPose( (cnovr_model*)b, &n->pose );
		}
		else
		{
			if( o->Render ) o->Render( objects[i] );
		}
	}
}

cnovr_header cnovr_node_header = {
	(cnovrfn)CNOVRNodeDelete, //Delete
	(cnovrfn)CNOVRNodeRender, //Render
	TYPE_NODE,
};



cnovr_simple_node * CNOVRNodeCreateSimple( int reserved_size )
{
	cnovr_simple_node * ret = malloc( sizeof( cnovr_simple_node ) );
	memset( ret, 0, sizeof( cnovr_simple_node ) );
	ret->base.header = &cnovr_node_header;
	ret->base.tccctx = TCCGetTag();
	ret->objects = 0;
	pose_make_identity( &ret->pose );
	return ret;
}

void CNOVRNodeAddObject( cnovr_simple_node * node, void * o )
{
	sb_push( node->objects, (cnovr_base *)o );
}

void CNOVRNodeRemoveObject( cnovr_simple_node * node, void * o )
{
	cnovr_base * obj = (cnovr_base *)o;
	int i;
	int count = sb_count( node->objects );
	for( i = 0; i < count; i++ )
	{
		if( obj == node->objects[i] )
		{
			node->objects[i] = 0;
			break;
		}
	}

	stb_sb_remove( node->objects, i );
}


