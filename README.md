# cnovr

**This is just me tinkering around with VR for fun in my own free time.  I'm not supporting this.  No, really.  This is a playground, you can come play, but it's probably broken for whatever you want to use it for, and don't expect me to fix it!**

If you want to chat about it, discord's 'cnovr' channel on the cnlohr-projects server is the best way: https://discord.gg/ZUWdwXk

My goal is to make a system which allows for extremely rapid VR game development, geared for livestreams.  In order to accomplish this, the primry mode for gameplay programming is to use C.  The whole of the engine is written in C to facilitate very fast recompiles.  All of the following assets may be modified at runtime and cnovr will constantly be scanning for file changes:
 * JSON "game" description files. (`.json`)
 * C scripts to be compiled by TinyCC (`.c`)
 * Shaders (`.vert`, `.frag`, `.geo`)
 * Models (currently only `.obj` supported)
 * Images/textures via stb_image (`.jpg`, `.png`, `.bmp`, `.pnm` etc..)

The overall methodology behind cnovr is to use a minimum of "libraries."  In general it should only be linking to OpenGL and OpenVR, where possible.  Things like video camera video will need to link to v4l2, etc...  From there it will directly open X windows, or Microsoft Windows windows.  All GL Extension rangling is handled by `chew.h`/`chew.c` internally.  By design, there are no eexternal dependencies that are not compiled directly into the executable.  Large compiled-in dependencies, apart from `TinyCC` and `stb_image` (and I'll think about maybe allowing gltf) will not be accepted.

All code is licensed under the MIT/X11 and/or newBSD licenses allowing for closed-source commercial use.

I'm probably never going to write much of a how-to or document anything on this project, either.  I'm mostly hoping google indexes the source code so I can google for when I want to do things I'm doing here.

## Operating

### On Linux:
#### Prerequisites:

```
sudo apt-get install build-essential libgl1-mesa-dev
```

#### Operating:
```
git clone https://github.com/cnlohr/cnovr --recurse-submodules
cd cnovr
cp openvr/lib/linux64/libopenvr_api.so .
make -j4
./main ovrball/ovrball.json
```

### On Windows

#### Prerequisites

* Download and install **64-bit** tinycc 0.9.27 to `C:\tcc` from here https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win64-bin.zip
* It **must** be the 64-bit version.  You cannot cross-compile cnovr on Windows.
* It **must** be the public binary version found in the link above (at least right now) as newer versions tweak the way some libraries work.

#### Operating:
```
git clone https://github.com/cnlohr/cnovr --recurse-submodules
cd cnovr
winbuild
main ovrball\ovrball.json
```


No OSX Support planned because there are no VR platforms for OSX. 


## TODO

Event System:
 * Should probably commonize event system.
 * Should probably make event system or at least lists be able to have priorities.
 * Should we rejigger the jobs task/list?  Should it ONLY be a list?  Removal will be slow, maybe we can mark the job as "unused" and go from there.

Collisions:
 * Add optimization for geometry early exit.
 * Add optimization for triangle early exit.
 * When intersecting with an object with a larger bubble, convex edges aren't perfectly smooth, space kinda wraps around them. and you intersect the edge even though you should the flat piece if you just extrude the face outward.  Need to also "stretch" the face.

Ovrball:
 * Write AI
 * Fix issues when you have a skipped timestamp.

New modules:
 * Terminal

Model Loading:
 * Optimize model loading
 * Add more features to custom effects for model loading the the colon operator.

Parts:
 * Make textures + shaders instancable, i.e. you make a new shader that's the same as an old one, it re-uses the object #.

General:
 * Make some global "now" or delta timer, "time of next frame flip"?  (i.e. 1/fps + frame start time)?
 * Figure out why OpenGL is crashing on Linux if you add the -rdynamic flag.  This is crucial for better debugging.

Maybe:
 * There should be some default shader for if a shader with a 0 id is activated, it can go to the default.
 * Give TCC scripts up to one frame to properly shutdown.  If they're still running, terminate them.  Callback?

Distant-future todo:
 * Improve YUYV / etc. decoding.  Also support other color formats.  Although most cameras support YUYV natively, the new Windows camera SDKs don't look like they do.  See this: https://docs.microsoft.com/en-us/previous-versions/ms867704(v=msdn.10)?redirectedfrom=MSDN
 * Input animations: { "name": "/actions/m/anim", "type": "skeleton", "skeleton": "/skeleton/hand/right" }
 * Check this out: https://github.com/NVIDIA/NvPipe/blob/master/src/Video_Codec_SDK_9.0.20/Samples/NvCodec/NvEncoder/NvEncoder.cpp  CNOVRStreaming?

Camera:
 * Improve cnv4l2 to allow for advanced userptr, to allow direct streaming into GPU.
 * Change the green-screen algorithm to more quickly perform the composite.  Probably by pyramidally calculating the green-pass.

Past Considerations:
 * The cnovr_header base.
   * Should this be a pointer to a structure of common pointers (like C++) or contain all basic props right there?
   * Decision: It should be like C++, where we have "types" in memory instead of willy nilly funptrs.
 * Should "update" be a phase?  Should "prerender" be a phase?  Should things just be loaded in as events?
   * Decision: Yes.  Update, Prerender and Collide should be decoupled since they are not order or tree dependent.
 * We want to be able to delete anything by a TCC instance handle to prevent serious issues.  How to handle this?
   * We could make the overarching system aware of it.
   * We could just wrap the TCC functions for these things to safe-ify them.

Livestream TODO:
 * People in chat can interface with the program.
 * Move ColorChord's audio system into cnovr.
 * Fix ball physics.

