# shm_opengl OBS Plugin

This allows OBS to synchronously capture frames from another app which is specifically writing out frames.

You will need to install `libobs-dev` from main or or `obs-studio` from the PPA.

This plugin opens a shared memory file which contains the following struct:

```
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
```




