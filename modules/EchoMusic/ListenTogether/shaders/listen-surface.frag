#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 accent;
    vec4 base;
    vec4 ink;
    vec4 geometry;
};
vec4 over(vec4 top, vec4 bottom)
{
    return top + bottom * (1.0 - top.a);
}
vec4 tint(vec4 color, float alpha)
{
    return color * alpha;
}
float linearAt(vec2 p, float degrees)
{
    vec2 direction = vec2(sin(radians(degrees)), -cos(radians(degrees)));
    float length = dot(abs(direction), geometry.xy);
    return clamp(dot(p - geometry.xy * .5, direction) / max(1.0, length) + .5, 0.0, 1.0);
}
void main()
{
    vec2 p = qt_TexCoord0 * geometry.xy;
    vec4 result;
    if (geometry.w < .5) {
        result = mix(mix(base, accent, .16), base, clamp(linearAt(p, 125.0) / .58, 0.0, 1.0));
        // The two source hero glows are static blurred circles, evaluated in
        // this quad instead of retaining two offscreen blur textures.
        vec2 a = vec2(geometry.x - 110.0, 20.0);
        vec2 b = vec2(geometry.x * .36 + 130.0, geometry.y + 90.0);
        float glowA = exp(-dot(p - a, p - a) / (2.0 * 106.0 * 106.0)) * .32;
        float glowB = exp(-dot(p - b, p - b) / (2.0 * 106.0 * 106.0)) * .32;
        result = over(tint(accent, glowA), result);
        result = over(vec4(.22, .741, .973, 1.0) * glowB, result);
        vec2 dotPosition = mod(p - vec2(2.0), 22.0);
        dotPosition = min(dotPosition, 22.0 - dotPosition);
        float dotMask = 1.0 - smoothstep(.65, 1.1, length(dotPosition));
        result = over(ink * (.15 * .32 * dotMask * clamp((qt_TexCoord0.x - .1) / .68, 0.0, 1.0)),
                      result);
    } else if (geometry.w < 1.5) {
        vec4 first = mix(vec4(.0784, .0824, .1098, 1.0), accent, .24);
        result = mix(first, vec4(.0824, .0941, .1294, 1.0), linearAt(p, 135.0));
        float radial = clamp(
            1.0 - length((p - vec2(geometry.x * .75, 0.0)) / max(geometry.xy, vec2(1.0))) / .48,
            0.0, 1.0);
        result = over(accent * (.48 * radial), result);
    } else {
        result = over(
            accent *
                (.18 *
                 clamp(1.0 - length((p - vec2(0, geometry.y)) / max(geometry.xy, vec2(1))) / .48,
                       0.0, 1.0)),
            base);
    }
    float radius = geometry.z;
    vec2 q = abs(p - geometry.xy * .5) - (geometry.xy * .5 - radius);
    if (geometry.w > .5 && geometry.w < 1.5 && p.y > geometry.y * .5)
        q.y = -1.0;
    float distance = length(max(q, vec2(0))) + min(max(q.x, q.y), 0.0) - radius;
    float coverage = clamp(.5 - distance / max(fwidth(distance), .0001), 0.0, 1.0);
    fragColor = result * (coverage * qt_Opacity);
}
