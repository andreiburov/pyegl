#version 430

// input from geometry shader
in FragmentData
{
    vec3 position;
    vec3 normal;
    vec4 color;
    vec2 uv;
    float mask;
    vec3 baryCoord;
    flat uvec3 vertexIds;
} fragData;

#ifdef TEXTURE_SHADING
uniform sampler2D color_texture;
#endif

// output buffers
layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec4 frag_position;
layout(location = 2) out vec4 frag_normal;
layout(location = 3) out vec2 frag_uv;
layout(location = 4) out vec4 frag_bary;
layout(location = 5) out vec4 frag_vertexIds;


void  main()
{
    if (fragData.mask < 0.5) discard;

    vec3 n = fragData.normal.xyz;
    if (n.z < 0.0) n *= -1.0;

    #ifdef CONSTANT_SHADING
    frag_color = fragData.color;
    #endif

    #ifdef PHONG_SHADING
    vec3 light = normalize(vec3(0.0, 1.0, 1.0));
    frag_color = fragData.color * (0.1 + max(dot(n, light), 0.0));
    #endif

    #ifdef TEXTURE_SHADING
    vec4 color = texture2D(color_texture, fragData.uv);
    frag_color = color;
    #endif

    frag_position = vec4(fragData.position.xyz, 1.0);
    frag_normal = vec4(n, 1.0);
    frag_uv = fragData.uv;
    frag_bary = vec4(fragData.baryCoord, 1.0);
    frag_vertexIds = vec4(fragData.vertexIds, 1.0);
}



/*
Note the interface blocks between the shaders need to have the same name!!

https://www.khronos.org/opengl/wiki/Fragment_Shader

IN-------------------------------------------------------------------------
-Fragment Shaders have the following built-in input variables.

in vec4 gl_FragCoord;
in bool gl_FrontFacing;
in vec2 gl_PointCoord;

-The space of gl_FragCoord can be modified by redeclaring gl_FragCoord with special input layout qualifiers:
    layout(origin_upper_left) in vec4 gl_FragCoord;
    This means that the origin for gl_FragCoord's window-space will be the upper-left of the screen, rather than the usual lower-left.

    layout(pixel_center_integer​) in vec4 gl_FragCoord;
    OpenGL window space is defined such that pixel centers are on half-integer boundaries. So the center of the lower-left pixel is (0.5, 0.5).


-OpenGL 4.0 and above define additional system-generated input values:

in int gl_SampleID;
in vec2 gl_SamplePosition;
in int gl_SampleMaskIn[];


-Some Fragment shader built-in inputs will take values specified by OpenGL, but these values can be overridden by user control.

in float gl_ClipDistance[];
in int gl_PrimitiveID;

-GL 4.3 provides the following additional inputs:

in int gl_Layer;
in int gl_ViewportIndex;

OUT------------------------------------------------------------------------

-Fragment Shaders have the following built-in output variables.

out float gl_FragDepth;

-GLSL 4.00 or ARB_sample_shading brings us:

out int gl_SampleMask[];
*/
