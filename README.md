# cnovr
CNLohr's OpenVR Foray

Prerequisites:

`
sudo apt-get install build-essential libgl1-mesa-dev
`

NOW:
 * Work on a focus system.

GAMEPLAN:
 * Model Loading
 * Terminal
 * Drag windows
 * Windows mirroring

NEWER:
 * Fix multisample?
 * Get intended frame time.
 * Make some global "now" or delta timer.
 * Should lists be allowed to have "priorities"?
 * There should be some default shader for if a shader with a 0 id is activated, it can go to the default.

Mid future todo:
 * Give TCC scripts up to one frame to properly shutdown.  If they're still running, terminate them.  Callback?
 * Fix Windows Crash Handler

Distant-future todo:
 * Input animations: { "name": "/actions/m/anim", "type": "skeleton", "skeleton": "/skeleton/hand/right" }

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


