# cnovr
Charles' OpenVR Foray

Prerequisites:

`
sudo apt-get install build-essential libgl1-mesa-dev
`

NEWER:
 * Clean up nodes created by dead TCCs


NEW:
 * Make there be a backup mechanism for the crash handler to abort.


TODO:
 * Make event system deletable/addable by tag. :: Make it fast
 * CONSIDER: Should we be able to instance TCC instances? Do we want another 'opaque' for Update/Render/etc.?

Considerations:
 * The cnovr_header base.
   * Should this be a pointer to a structure of common pointers (like C++) or contain all basic props right there?
   * Decision: It should be like C++
 * Should "update" be a phase?  Should "prerender" be a phase?  Should things just be loaded in as events?
   * Decision: Yes.  Update, Prerender and Collide should be decoupled since they are not order or tree dependent.
 * We want to be able to delete anything by a TCC instance handle to prevent serious issues.  How to handle this?
   * We could make the overarching system aware of it.
   * We could just wrap the TCC functions for these things to safe-ify them.


