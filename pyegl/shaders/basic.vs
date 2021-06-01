#version 430

// input uniforms
uniform mat4 modelview;
uniform mat4 projection;
uniform vec4 mesh_normalization; // center of gravity, scale


// input mesh data
layout(location = 0) in vec4  in_position;
layout(location = 1) in vec4  in_normal;
layout(location = 2) in vec4  in_color;
layout(location = 3) in vec2  in_uv;
layout(location = 4) in float  in_mask;

// output to geometry shader
out VertexData
{
  vec3 position;
  vec3 normal;
  vec4 color;
  vec2 uv;
  float mask;
  uint id;
} outData;

void main()
{
  vec4 pos = vec4(in_position.xyz, 1.0);

  outData.position = pos.xyz;
  outData.normal = in_normal.xyz;
  outData.color = in_color;
  outData.uv = in_uv;
  outData.mask = in_mask;
  outData.id = gl_VertexID;

  pos = modelview * vec4(outData.position, 1.0);
  gl_Position = projection * pos;
}
