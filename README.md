# cnovr
CNLohr's OpenVR Foray

Prerequisites:

`
sudo apt-get install build-essential libgl1-mesa-dev
`

NOW NOW:
 Why:
==5880== Thread 4:
==5880== Invalid write of size 4
==5880==    at 0x14CFB6: CNOVRJobProcessor (cnovrutil.c:820)
==5880==    by 0x59A96DA: start_thread (pthread_create.c:463)
==5880==    by 0x5F1F88E: clone (clone.S:95)
==5880==  Address 0x9ff2a5c is 204 bytes inside a block of size 240 free'd
==5880==    at 0x4C30D3B: free (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==5880==    by 0x14CA94: DeleteLaterFrameCb (cnovrutil.c:1184)
==5880==    by 0x14EA66: CNOVRListCall (cnovrutil.c:1083)
==5880==    by 0x145099: CNOVRUpdate (cnovr.c:232)
==5880==    by 0x15469B: main (main.c:26)
==5880==  Block was alloc'd at
==5880==    at 0x4C2FB0F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==5880==    by 0x152D42: TCCmalloc (cnovrtccinterface.c:365)
==5880==    by 0x14178C: tcccrash_getcheckpoint (tcccrash.c:376)
==5880==    by 0x14CFB5: CNOVRJobProcessor (cnovrutil.c:820)
==5880==    by 0x59A96DA: start_thread (pthread_create.c:463)
==5880==    by 0x5F1F88E: clone (clone.S:95)
==5880== 
==5880== Invalid write of size 8
==5880==    at 0x5E3CB70: __sigsetjmp (setjmp.S:26)
==5880==    by 0x14CFD6: CNOVRJobProcessor (cnovrutil.c:820)
==5880==    by 0x59A96DA: start_thread (pthread_create.c:463)
==5880==    by 0x5F1F88E: clone (clone.S:95)
==5880==  Address 0x9ff2990 is 0 bytes inside a block of size 240 free'd
==5880==    at 0x4C30D3B: free (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==5880==    by 0x14CA94: DeleteLaterFrameCb (cnovrutil.c:1184)
==5880==    by 0x14EA66: CNOVRListCall (cnovrutil.c:1083)
==5880==    by 0x145099: CNOVRUpdate (cnovr.c:232)
==5880==    by 0x15469B: main (main.c:26)
==5880==  Block was alloc'd at
==5880==    at 0x4C2FB0F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==5880==    by 0x152D42: TCCmalloc (cnovrtccinterface.c:365)
==5880==    by 0x14178C: tcccrash_getcheckpoint (tcccrash.c:376)
==5880==    by 0x14CFB5: CNOVRJobProcessor (cnovrutil.c:820)
==5880==    by 0x59A96DA: start_thread (pthread_create.c:463)
==5880==    by 0x5F1F88E: clone (clone.S:95)
==5880== 
==5880== Invalid write of size 8
==5880==    at 0x5E3CB83: __sigsetjmp (setjmp.S:37)
==5880==    by 0x14CFD6: CNOVRJobProcessor (cnovrutil.c:820)
==5880==    by 0x59A96DA: start_thread (pthread_create.c:463)
==5880==    by 0x5F1F88E: clone (clone.S:95)
==5880==  Address 0x9ff2998 is 8 bytes inside a block of size 240 free'd
==5880==    at 0x4C30D3B: free (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==5880==    by 0x14CA94: DeleteLaterFrameCb (cnovrutil.c:1184
NOW:
 * TCC Instances should have sustaining data options.

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
 * When shutting down TCC, the actual destruction should maybe happen a frame later to give TCC a chance to actually shutdown instead of us canceling threads mid-run.
 * Should models have a transform matrix?

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


