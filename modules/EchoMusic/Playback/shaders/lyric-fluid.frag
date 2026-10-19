#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float aspect;
    float rhythm;
    float fade;
    vec4 baseColor;
    float backgroundOpacity;
};
layout(binding = 1) uniform sampler2D source;
// Static blur and rhythm share one frozen, bounded blur texture. Only this
// final quad covers the window; animation never regenerates the blur pyramid.
vec4 sampleCover(vec2 uv)
{
    if (rhythm < 0.5) {
        // Match the static background's original 1.2x overscan.
        return texture(source, (uv - 0.5) / 1.2 + 0.5);
    }
    vec2 flow = vec2(sin(uv.y * 4.0 + time), cos(uv.x * 3.0 - time)) * 0.12;
    vec4 a = texture(source, clamp(uv * 0.6 + flow + vec2(0.1, 0.1), 0.0, 1.0));
    vec4 b = texture(source, clamp(vec2(1.0 - uv.y, uv.x) * 0.6 - flow + vec2(0.2), 0.0, 1.0));
    vec4 c = texture(source, clamp(vec2(uv.y, 1.0 - uv.x) + flow * 0.7, 0.0, 1.0));
    float weight = 0.5 + 0.5 * sin(time * 0.5 + uv.x * aspect + uv.y * 2.0);
    return mix(mix(a, b, weight), c, 0.25);
}

void main()
{
    // Compose the artwork fade, opaque page base and 50% black scrim here.
    // Fade the finished background once without allocating a window-sized layer.
    vec4 art = sampleCover(qt_TexCoord0) * fade;
    vec4 background = art + baseColor * (1.0 - art.a);
    background.rgb *= 0.5;
    fragColor = background * backgroundOpacity * qt_Opacity;
}
