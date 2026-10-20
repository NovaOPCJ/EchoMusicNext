#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 step;
    float grade;
};
layout(binding = 1) uniform sampler2D source;
void main()
{
    if (grade > 0.5) {
        vec4 art = texture(source, qt_TexCoord0);
        vec3 color = art.a > 0.0 ? art.rgb / art.a : vec3(0);
        // AMLL's contrast .4 -> saturation 3 -> contrast 1.7 -> brightness .75,
        // with one 8-bit clamp before the four radius-two box blur iterations.
        color = (color - 128.0 / 255.0) * 0.4 + 128.0 / 255.0;
        float gray = dot(color, vec3(.3, .59, .11));
        color = ((3.0 * color - 2.0 * gray - 128.0 / 255.0) * 1.7 + 128.0 / 255.0) * .75;
        color = round(clamp(color, 0.0, 1.0) * 255.0) / 255.0;
        fragColor = vec4(color * art.a, art.a);
    } else {
        vec4 sum = vec4(0);
        for (int i = -2; i <= 2; ++i)
            sum += texture(
                source, clamp(qt_TexCoord0 + float(i) * step, vec2(0.5 / 32.0), vec2(31.5 / 32.0)));
        fragColor = sum / 5.0;
    }
}
