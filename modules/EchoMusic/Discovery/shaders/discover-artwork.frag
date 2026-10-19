// SPDX-License-Identifier: GPL-3.0-only
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 sourceRect;
    vec3 viewport;
};
layout(binding = 1) uniform sampler2D source;

// Retain the source's smooth top entrance. The user requested a clearer lower
// cover: only the separate legibility scrim shades it, without a second fade.
void main()
{
    float t = qt_TexCoord0.y;
    float alpha;
    if (t < 0.05)
        alpha = mix(0.0, 0.10, t / 0.05);
    else if (t < 0.12)
        alpha = mix(0.10, 0.45, (t - 0.05) / 0.07);
    else if (t < 0.20)
        alpha = mix(0.45, 0.82, (t - 0.12) / 0.08);
    else if (t < 0.28)
        alpha = mix(0.82, 1.0, (t - 0.20) / 0.08);
    else
        alpha = 1.0;
    // Bottom corners match the shell material. Upper corners join the titlebar
    // and stay square. Derivatives keep the edge smooth at native UI scales.
    vec2 p = qt_TexCoord0 * viewport.xy;
    float r = viewport.z;
    vec2 corner =
        max(vec2(abs(p.x - viewport.x * 0.5) - (viewport.x * 0.5 - r), p.y - (viewport.y - r)),
            vec2(0.0));
    float distance = length(corner) - r;
    float coverage = clamp(0.5 - distance / max(fwidth(distance), 0.0001), 0.0, 1.0);
    vec2 uv = sourceRect.xy + qt_TexCoord0 * sourceRect.zw;
    fragColor = texture(source, uv) * (alpha * coverage * qt_Opacity);
}
