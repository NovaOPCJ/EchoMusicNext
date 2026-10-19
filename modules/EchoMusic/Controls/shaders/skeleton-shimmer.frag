// SPDX-License-Identifier: GPL-3.0-only
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 shimmerColor;
    vec2 viewSize;
    float cornerRadius;
    float phase;
};
void main()
{
    vec2 halfSize = viewSize * 0.5;
    float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
    vec2 q = abs(qt_TexCoord0 * viewSize - halfSize) - halfSize + radius;
    float distance = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    float edge = max(fwidth(distance), 0.001);
    float mask = 1.0 - smoothstep(-edge * 0.5, edge * 0.5, distance);
    float highlight = max(0.0, 1.0 - abs(qt_TexCoord0.x - (phase * 2.0 - 1.0)));
    fragColor = shimmerColor * (qt_Opacity * highlight * mask);
}
