# cnovr
CNLohr's OpenVR Foray

Prerequisites:

```
sudo apt-get install build-essential libgl1-mesa-dev
```

## Operating

On Linux:
```
git clone https://github.com/cnlohr/cnovr --recurse-submodules
cd cnovr
cp openvr/lib/linux64/libopenvr_api.so .
make -j4
./main ovrball/ovrball.json
```

On Windows, download and install tinycc 0.9.27 to C:\tcc.  Then,
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


