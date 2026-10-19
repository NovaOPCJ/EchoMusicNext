#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float aspect;
    float coverAspect;
    float rhythm;
};
layout(binding = 1) uniform sampler2D source;
vec2 rotate(vec2 p, float angle)
{
    float s = sin(angle), c = cos(angle);
    return mat2(c, s, -s, c) * p;
}
float hash(vec2 p)
{
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float noise(vec2 p)
{
    vec2 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(cell), hash(cell + vec2(1, 0)), f.x),
               mix(hash(cell + vec2(0, 1)), hash(cell + vec2(1, 1)), f.x), f.y) -
           0.5;
}
vec4 coverAt(vec2 uv)
{
    // Preserve the original cover aspect ratio, including non-square artwork.
    vec2 crop = coverAspect > 1.0 ? vec2(1.0 / coverAspect, 1) : vec2(1, coverAspect);
    return texture(source, clamp((uv - 0.5) * crop + 0.5, 0.0, 1.0));
}
void main()
{
    if (rhythm < 0.5) {
        vec2 crop =
            aspect > coverAspect ? vec2(1, coverAspect / aspect) : vec2(aspect / coverAspect, 1);
        fragColor = texture(source, (qt_TexCoord0 - 0.5) * crop / 1.2 + 0.5);
        return;
    }
    // EchoMusic's four independently rotating cover quadrants and slow outer
    // rotation. All noise/trigonometry runs in this tiny composition target.
    const float tau = 6.28318530718;
    vec2 p = (qt_TexCoord0 - 0.5) * vec2(min(aspect, 1.0), min(1.0 / aspect, 1.0));
    p = rotate(p, -time * tau / 150.0) / 1.2;
    p += vec2(noise(p * 5.0 + 0.5), noise(p * 5.0 + 19.0)) * 0.22;
    vec4 result = vec4(0);
    for (int i = 0; i < 4; ++i) {
        vec2 tile = vec2(float(i % 2), float(i / 2));
        vec2 center = (tile * 2.0 - 1.0) * 0.24745;
        vec2 local = rotate(p - center, (time + float(i) * 5.0) * tau / 60.0) / 0.707 + 0.5;
        vec2 edge =
            smoothstep(vec2(0), vec2(0.06), local) * (1.0 - smoothstep(vec2(0.94), vec2(1), local));
        vec4 pixel = coverAt((tile + clamp(local, 0.005, 0.995)) * 0.5);
        pixel *= edge.x * edge.y;
        result = pixel + result * (1.0 - pixel.a);
    }
    fragColor = result;
}
