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

// uniforms
#ifdef TEXTURE_SHADING
uniform sampler2D color_texture;
#endif

uniform vec3 ambient_light;
uniform vec3 brightness;
uniform vec3 light_direction;

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
    frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    
    vec3 base_color = clamp(fragData.color.rgb + brightness, 0.0, 1.0);

    #ifdef TEXTURE_SHADING
    base_color = clamp(texture2D(color_texture, fragData.uv).rgb + brightness, 0.0, 1.0);
    #endif

    vec3 n = fragData.normal.xyz;
    if (n.z < 0.0) n *= -1.0;

    #ifdef CONSTANT_SHADING
    frag_color += vec4(base_color * ambient_light, 0.0);
    #endif

    #ifdef DIFFUSE_SHADING
    vec3 light = normalize(light_direction);
    float diffuse = max(dot(n, light), 0.0);
    frag_color += vec4(base_color * diffuse, 0.0);
    #endif

    #if !defined(CONSTANT_SHADING) && ! defined(DIFFUSE_SHADING)
    frag_color = vec4(base_color, 1.0);
    #endif

    frag_color = clamp(frag_color, 0.0, 1.0);
    frag_position = vec4(fragData.position.xyz, 1.0);
    frag_normal = vec4(n, 1.0);
    frag_uv = fragData.uv;
    frag_bary = vec4(fragData.baryCoord, 1.0);
    frag_vertexIds = vec4(fragData.vertexIds, 1.0);
}
