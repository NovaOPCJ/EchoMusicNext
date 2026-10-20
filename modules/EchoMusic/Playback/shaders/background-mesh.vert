#version 440
layout(location = 0) in vec4 vertex;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec2 texCoord;
layout(std140, binding = 0) uniform buf
{
    mat4 matrix;
    float opacity;
    vec4 pose;
};
void main()
{
    texCoord = uv;
    gl_Position = matrix * vertex;
}
