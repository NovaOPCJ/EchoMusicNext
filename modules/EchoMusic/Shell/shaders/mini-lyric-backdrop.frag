#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 baseColor;
    vec2 extent;
    float coverAspect;
    float cornerRadius;
    float roundTop;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    vec2 uv = qt_TexCoord0;
    float aspect = extent.x / max(1.0, extent.y);
    vec2 crop = vec2(min(1.0, aspect / coverAspect), min(1.0, coverAspect / aspect));
    vec4 art = texture(source, (uv - 0.5) * crop + 0.5) * 0.22;
    vec4 background = art + baseColor * (1.0 - art.a);
    vec4 scrim = baseColor * mix(0.84, 0.95, uv.y);
    vec4 color = scrim + background * (1.0 - scrim.a);
    // Only the panel's outside corners are rounded. The edge adjoining the
    // control bar is flat, exactly meeting the card without a rectangular inset.
    float radius = ((uv.y < 0.5) == (roundTop > 0.5)) ? cornerRadius : 0.0;
    vec2 q = abs(uv * extent - extent * 0.5) - (extent * 0.5 - radius);
    float distance = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
    float aa = max(fwidth(distance), 0.001);
    float mask = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, distance);
    fragColor = color * mask * qt_Opacity;
}
