//uniforms that are used several places are placed here.

// #MAPUNIFORM is a special flag that tells cnovr to load the named uniform into a slot
// this is like the layout= thing that was provided in later versions of OpenGL.
// but I want to support as broad of a platform as possible, so we have to reinvent the wheel.

//Locations 0..3 TBA
uniform mat4 umModel;           //#MAPUNIFORM umModel 4
uniform mat4 umView;            //#MAPUNIFORM umView 5
uniform mat4 umPerspective;     //#MAPUNIFORM umPerspective 6
uniform vec4 ufRenderProps;     //#MAPUNIFORM ufRenderProps 7
uniform sampler2D textures[8];  //#MAPUNIFORM textures 8

//Locations 16+ are fair game by users.

