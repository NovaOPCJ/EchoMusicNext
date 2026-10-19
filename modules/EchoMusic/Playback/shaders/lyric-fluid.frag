#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    float fade;
    vec4 baseColor;
    float backgroundOpacity;
    float rhythm;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    // One full-window sample. Composition/motion and the strong blur have
    // already finished in three small RGBA16F targets, at most 30 times/sec.
    vec4 art = texture(source, qt_TexCoord0);
    if (rhythm > 0.5) {
        float luma = dot(art.rgb, vec3(0.2126, 0.7152, 0.0722));
        art.rgb = clamp(mix(vec3(luma), art.rgb, 1.3) * 1.5, 0.0, art.a) * 0.76;
    }
    art *= fade;
    vec4 background = art + baseColor * (1.0 - art.a);
    background.rgb *= 0.5;
    // Stable sub-LSB triangular dither prevents SDR banding when magnifying
    // the smooth half-float gradients. No animated grain or extra render pass.
    vec2 pixel = floor(gl_FragCoord.xy);
    float a = fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
    float b = fract(52.9829189 * fract(dot(pixel + 17.0, vec2(0.06711056, 0.00583715))));
    background.rgb += (a - b) / 255.0 * background.a;
    fragColor = background * backgroundOpacity * qt_Opacity;
}
