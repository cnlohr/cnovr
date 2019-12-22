#include <obs/obs-module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/limits.h> //for PATH_MAX
#include <GL/glxext.h>
#include <dlfcn.h>
#include <pwd.h>
#include <unistd.h>

// Defines common functions (required)
OBS_DECLARE_MODULE()


typedef struct
{
	uint32_t magic; //0xAABB0100 + version number (Currently 0)
	uint32_t source_pid;
	uint32_t handlecount;
	uint32_t handlehead;
	uint32_t graphicstype; //0 for opengl.
	uint32_t enumtype;     //RENDERBUFFER or TEXTURE
	uint32_t width;
	uint32_t height;
	uint64_t context;      //A GLX texture.
	uint8_t reserved[24];
	//64 bytes in.
	uint64_t handles[0];
} obs_gl_shm;


struct shm_opengl {
	uint32_t width;
	uint32_t height;
	char file[PATH_MAX];
	GLXContext xctx;
	GLXContext remotecontext;
	GLXContextID remotecontextidlast;
	Display * current_display;
	obs_source_t *src;
	int last_blit_frame_no;
	int file_dev;
	obs_gl_shm * shm_dev;
	PFNGLXCOPYIMAGESUBDATANVPROC copyext;
	PFNGLXIMPORTCONTEXTEXTPROC   importext;
};

#define obs_module_text(x) x

static const char *shm_opengl_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ShmOpenGL");
}

static void shm_opengl_update(void *data, obs_data_t *settings)
{
	struct shm_opengl *context = data;
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
	const char *file = obs_data_get_string(settings, "file");

	strncpy( context->file, file, PATH_MAX );
	context->width = width;
	context->height = height;
}

static void *shm_opengl_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);

	struct shm_opengl *context = bzalloc(sizeof(struct shm_opengl));
	context->src = source;

	shm_opengl_update(context, settings);

	return context;
}

static void shm_opengl_destroy(void *data)
{
	bfree(data);
}

static obs_properties_t *shm_opengl_properties(void * data)
{
	struct shm_opengl *s = data;

	obs_properties_t *props = obs_properties_create();


	obs_properties_add_path(props, "file", obs_module_text("File"),
				OBS_PATH_FILE, "obsglshm (*.obsglshm);;All Files (*.*)", s->file?s->file:"");

	obs_properties_add_int(props, "width",
			       obs_module_text("ColorSource.Width"), 0, 4096,
			       1);

	obs_properties_add_int(props, "height",
			       obs_module_text("ColorSource.Height"), 0, 4096,
			       1);

	return props;
}

static void shm_opengl_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct shm_opengl *context = data;
	GLXContext glxcontext = context->xctx;
	if( !glxcontext ) context->xctx = glxcontext = glXGetCurrentContext();
	GLint drawFboId = 0;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFboId);

	if( !context->current_display )
	{
		context->current_display = glXGetCurrentDisplay();
	}

	if( drawFboId == GL_INVALID_ENUM || drawFboId == GL_INVALID_VALUE || !glxcontext || !context->current_display )
	{
		//Error.
		glClearColor (1, 0.5, glxcontext?1.0:0.0, 1);
		glClear (GL_COLOR_BUFFER_BIT);
		return;
	}


	if( ( !context->file_dev || context->file_dev == -1 ) && context->file[0] )
	{
		context->file_dev = open( context->file, O_RDONLY );
	}
	if( ( !context->shm_dev || context->shm_dev == (void*)-1 ) && context->file_dev > 0 )
	{
		context->shm_dev = mmap( NULL, sizeof( obs_gl_shm ), PROT_READ, MAP_SHARED, context->file_dev, 0);
	}

	if( !context->shm_dev || context->shm_dev == (void*)-1 )
	{
		glClearColor (1, 0, context->file_dev?1.0:0.0, 1);
		glClear (GL_COLOR_BUFFER_BIT);
		return;
	}

	if( !context->copyext )
	{
		context->copyext = dlsym( 0, "glXCopyImageSubDataNV" );

		if( !context->copyext )
		{
			glClearColor( 1., 0.5, 0.5, 1. );
			glClear( GL_COLOR_BUFFER_BIT );
			return;
		}
	}

	if( !context->importext )
	{
		context->importext = dlsym( 0, "glXImportContextEXT" );

		if( !context->importext )
		{
			glClearColor( 1., 0.5, 0.5, 1. );
			glClear( GL_COLOR_BUFFER_BIT );
			return;
		}
	}

	obs_gl_shm * sd = context->shm_dev;

	if( context->remotecontextidlast != sd->context )
	{
		context->remotecontext = context->importext( context->current_display, sd->context );
		context->remotecontextidlast = sd->context;
		printf( "** IMP %p %lx -> %p\n", context->current_display, sd->context, context->remotecontext );
	}

	//context->remotecontext = (void*)((uint8_t*)sd) + 4096);

	int hc = sd->handlecount;
	int hh = sd->handlehead;
	int blitno = context->last_blit_frame_no;
	int diff = (hh - blitno + hc)%hc;

	if( diff == 0 )
	{
		blitno = blitno;
	}
	else if( diff > 2 ) //Prevent us from falling too far behind.
	{
		blitno+=2;
	}
	else
	{
		blitno++;
	}
	blitno = blitno % hc;

	printf( "%p / %p, %p //SDC: %lx // %p\n", context, glxcontext, sd,sd->context, context->remotecontext );
	printf( "%p(%p, %p, %ld, %d, 0, 0, 0, 0, %p, %d, %d, 0, 0, 0, 0, %d %d 1\n",
		context->copyext, context->current_display, /*context->remotecontext*/((void*)((uint8_t*)sd) + 4096), sd->handles[blitno], sd->enumtype,
		glxcontext, drawFboId, GL_FRAMEBUFFER, 
		context->width, context->height );

#if 1
	(context->copyext)( context->current_display, 
		/*context->remotecontext*/((void*)((uint8_t*)sd) + 4096), sd->handles[blitno], sd->enumtype, 0, 0, 0, 0,
		glxcontext, drawFboId, GL_FRAMEBUFFER, 0, 0, 0, 0,
		context->width, context->height, 1 );
#endif
		
	context->last_blit_frame_no = blitno;
	//Otherwise, we now have an FBO ID, and a GLX Conext.
	//Perform logic and copy;
	//Use context->last_blit_frame_no in conjunction with mmap'd thing.
}

static uint32_t shm_opengl_getwidth(void *data)
{
	struct shm_opengl *context = data;
	return context->width;
}

static uint32_t shm_opengl_getheight(void *data)
{
	struct shm_opengl *context = data;
	return context->height;
}

static void shm_opengl_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);

	struct passwd *pw = getpwuid(getuid());
	char cts[1024];
	snprintf( cts, sizeof(cts)-1, "%s/.obsglshm/", pw->pw_dir );
	obs_data_set_default_string(settings, "file", cts );
}


struct obs_source_info shm_opengl_info = {
	.id = "shm_opengl",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.create = shm_opengl_create,
	.destroy = shm_opengl_destroy,
	.update = shm_opengl_update,
	.get_name = shm_opengl_get_name,
	.get_defaults = shm_opengl_defaults,
	.get_width = shm_opengl_getwidth,
	.get_height = shm_opengl_getheight,
	.video_render = shm_opengl_render,
	.get_properties = shm_opengl_properties,
	//.icon_type = OBS_ICON_TYPE_COLOR,
};


bool obs_module_load(void)
{
	obs_register_source(&shm_opengl_info);
	return true;
}


