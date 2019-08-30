# cnovr
Charles' OpenVR Foray

Prerequisites:

`
sudo apt-get install build-essential libgl1-mesa-dev
`

TODO:
 * Make event system deletable/addable by tag. :: Make it fast
 * CONSIDER: Should we be able to instance TCC instances?

Considerations:
 * The cnovr_header base.
   * Should this be a pointer to a structure of common pointers (like C++) or contain all basic props right there?
   * Decision: It should be like C++
 * Should "update" be a phase?  Should "prerender" be a phase?  Should things just be loaded in as events?
