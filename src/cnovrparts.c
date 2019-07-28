#include <cnovrparts.h>
#include <chew.h>
#include <string.h>
#include <stdlib.h>
 

static void CNOVRDeleteRenderFrameBuffer( cnovr_rf_buffer * ths )
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
	ret->header.Delete = (cnovrfn)CNOVRDeleteRenderFrameBuffer;

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

