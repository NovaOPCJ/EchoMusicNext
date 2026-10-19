#version 440
layout(location = 0) in vec2 vertex;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 revealData;
layout(location = 3) in vec2 effectsData;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 reveal;
layout(location = 2) out vec2 effects;
layout(std140, binding = 0) uniform buf
{
    mat4 matrix;
    float opacity;
    vec4 bright;
    vec4 dim;
};
void main()
{
    uv = texCoord;
    reveal = revealData;
    effects = effectsData;
    gl_Position = matrix * vec4(vertex, 0.0, 1.0);
}
