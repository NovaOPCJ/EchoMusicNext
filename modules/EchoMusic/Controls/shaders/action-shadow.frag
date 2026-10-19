// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only Based on Qt 6.11.2 RectangularShadow's documented material. The added source cutout
// matches CSS outer box-shadow without darkening translucent controls.
#version 440
layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec2 fragCoord;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    vec3 iResolution;
    vec2 rectSize;
    float radius;
    float blur;
    vec2 sourceSize;
    vec2 sourceOffset;
    float sourceRadius;
};
float roundedBox(vec2 position, vec2 size, float r)
{
    vec2 q = abs(position) - size + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main()
{
    vec2 position = fragCoord - iResolution.xy * 0.5;
    float distance = roundedBox(position, rectSize, radius);
    float alpha = 1.0 - smoothstep(0.0, blur, distance);
    float sourceDistance = roundedBox(position + sourceOffset, sourceSize * 0.5, sourceRadius);
    float outside = smoothstep(-0.5, 0.5, sourceDistance);
    fragColor = color * qt_Opacity * alpha * alpha * outside;
}
