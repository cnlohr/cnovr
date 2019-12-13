#extension GL_ARB_explicit_attrib_location: enable
#extension GL_ARB_explicit_uniform_location : enable

layout (location = 4) uniform mat4 umModel;
layout (location = 5) uniform mat4 umView;
layout (location = 6) uniform mat4 umPerspective;
layout (location = 7) uniform vec4 ufRenderProps;
layout (location = 8) uniform sampler2D textures[8];


