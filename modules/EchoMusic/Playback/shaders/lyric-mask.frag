#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 reveal;
layout(location = 2) in vec2 effects;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 matrix;
    float opacity;
    vec4 bright;
    vec4 dim;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    vec2 mask = texture(source, uv).rg;
    float x = effects.x > 0.0 ? 1.0 - reveal.x : reveal.x;
    float progress = clamp((reveal.y - x) / reveal.z + 0.5, 0.0, 1.0);
    vec4 color = mix(dim, bright, progress);
    float ink = mask.r * color.a;
    float halo = mask.g * effects.y * bright.a;
    fragColor =
        vec4(color.rgb * ink + bright.rgb * halo * (1.0 - ink), ink + halo * (1.0 - ink)) * opacity;
}
