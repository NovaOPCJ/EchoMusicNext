// SPDX-License-Identifier: GPL-3.0-only
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 tintColor;
    float peakAlpha;
    float midAlpha;
    float ditherGain;
};

// Stable raster-pixel decorrelation rather than a scaled QML noise image. No
// time/phase input: an idle background neither shimmers nor schedules frames.
float quantizationNoise()
{
    // Interleaved gradient noise (Jimenez). Arithmetic-only so the same qsb
    // also bakes for the project's GLSL ES 100 / GLSL 120 targets.
    vec2 p = floor(gl_FragCoord.xy);
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))) - 0.5;
}
void main()
{
    float t = clamp(qt_TexCoord0.y, 0.0, 1.0);
    float alpha;
    if (t < 0.22)
        alpha = mix(peakAlpha, peakAlpha * 0.94, t / 0.22);
    else if (t < 0.55)
        alpha = mix(peakAlpha * 0.94, midAlpha, (t - 0.22) / 0.33);
    else if (t < 0.802)
        alpha = mix(midAlpha, midAlpha * 0.3, (t - 0.55) / 0.252);
    else
        alpha = mix(midAlpha * 0.3, 0.0, (t - 0.802) / 0.198);
    vec4 color = tintColor * (alpha * qt_Opacity);
    // Keep premultiplied-alpha bounds and fade to exact transparency. Dither
    // RGB immediately before SDR blending; changing alpha alone cannot break
    // the low-contrast panel's final quantization plateaus.
    float noise =
        quantizationNoise() * (ditherGain / 255.0) * smoothstep(0.0, ditherGain / 255.0, color.a);
    color.rgb = clamp(color.rgb + vec3(noise), vec3(0.0), vec3(color.a));
    fragColor = color;
}
