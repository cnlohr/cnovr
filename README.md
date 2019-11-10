# cnovr
CNLohr's OpenVR Foray

Prerequisites:

`
sudo apt-get install build-essential libgl1-mesa-dev
`

TODO:
 * Add optimization for geometry early exit.
 * Add optimization for triangle early exit.

HMM:
 * Should we make the "remove callback" more generic for any kind of callback?
 * Should we have some convenient object thing?

NOW:
 * Windows Windows
 * Rawdraw 2 extra modes.

GAMEPLAN:
 * Make camera you can move around.
 * Model Loading
 * Terminal
 * Make textures + shaders instancable, i.e. you make a new shader that's the same as an old one, it re-uses the object #.

NEWER:
 * Make some global "now" or delta timer.
 * Should lists be allowed to have "priorities"?
 * There should be some default shader for if a shader with a 0 id is activated, it can go to the default.
 * When shutting down TCC, the actual destruction should maybe happen a frame later to give TCC a chance to actually shutdown instead of us canceling threads mid-run.
 * Should models have a transform matrix?

Mid future todo:
 * Give TCC scripts up to one frame to properly shutdown.  If they're still running, terminate them.  Callback?

Distant-future todo:
 * Input animations: { "name": "/actions/m/anim", "type": "skeleton", "skeleton": "/skeleton/hand/right" }
 * Check this out: https://github.com/NVIDIA/NvPipe/blob/master/src/Video_Codec_SDK_9.0.20/Samples/NvCodec/NvEncoder/NvEncoder.cpp  CNOVRStreaming?

Further consider (long-term)
 * Should we continue using "Tag" as an ambiguous term meaning "TCC Instance" or "Node" or should we standardize?
 * Should we rejigger the jobs task/list?  Should it ONLY be a list?  Removal will be slow, maybe we can mark
    the job as "unused" and go from there.
 * Should we refcount objects?  I don't particularly like that idea.

Considerations:
 * The cnovr_header base.
   * Should this be a pointer to a structure of common pointers (like C++) or contain all basic props right there?
   * Decision: It should be like C++, where we have "types" in memory instead of willy nilly funptrs.
 * Should "update" be a phase?  Should "prerender" be a phase?  Should things just be loaded in as events?
   * Decision: Yes.  Update, Prerender and Collide should be decoupled since they are not order or tree dependent.
 * We want to be able to delete anything by a TCC instance handle to prevent serious issues.  How to handle this?
   * We could make the overarching system aware of it.
   * We could just wrap the TCC functions for these things to safe-ify them.


