#version 440
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 matrix;
    float opacity;
    vec4 pose;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    vec2 p = texCoord - 0.2;
    vec2 uv = vec2(pose.x * p.x - pose.y * p.y, pose.y * p.x + pose.x * p.y) + 0.5;
    // Explicit mirrored sampling also works for RHI backends which clamp an
    // offscreen source's native sampler. No GL-specific wrapping dependency.
    uv = 1.0 - abs(mod(uv, 2.0) - 1.0);
    vec4 art = texture(source, uv);
    float vignette = 1.0 - smoothstep(0.3, 0.8, distance(texCoord, vec2(0.5)));
    art.rgb *= 0.6 + 0.4 * vignette;
    fragColor = art * opacity;
}
