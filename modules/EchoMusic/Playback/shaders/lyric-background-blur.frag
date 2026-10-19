#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf
{
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 step;
};
layout(binding = 1) uniform sampler2D source;
// Normalized sigma=10 physical texels, radius=24. Bilinear-paired taps evaluate
// the 49-tap separable Gaussian with 25 texture fetches per bounded pixel.
void main()
{
    vec4 color = texture(source, qt_TexCoord0) * 0.0404707400;
    color += texture(source, qt_TexCoord0 + step * 1.49625007) * 0.0799382571;
    color += texture(source, qt_TexCoord0 - step * 1.49625007) * 0.0799382571;
    color += texture(source, qt_TexCoord0 + step * 3.49125089) * 0.0760491272;
    color += texture(source, qt_TexCoord0 - step * 3.49125089) * 0.0760491272;
    color += texture(source, qt_TexCoord0 + step * 5.48625347) * 0.0695193063;
    color += texture(source, qt_TexCoord0 - step * 5.48625347) * 0.0695193063;
    color += texture(source, qt_TexCoord0 + step * 7.48125878) * 0.0610644208;
    color += texture(source, qt_TexCoord0 - step * 7.48125878) * 0.0610644208;
    color += texture(source, qt_TexCoord0 + step * 9.47626785) * 0.0515397898;
    color += texture(source, qt_TexCoord0 - step * 9.47626785) * 0.0515397898;
    color += texture(source, qt_TexCoord0 + step * 11.47128164) * 0.0417992602;
    color += texture(source, qt_TexCoord0 - step * 11.47128164) * 0.0417992602;
    color += texture(source, qt_TexCoord0 + step * 13.46630116) * 0.0325736221;
    color += texture(source, qt_TexCoord0 - step * 13.46630116) * 0.0325736221;
    color += texture(source, qt_TexCoord0 + step * 15.46132739) * 0.0243913009;
    color += texture(source, qt_TexCoord0 - step * 15.46132739) * 0.0243913009;
    color += texture(source, qt_TexCoord0 + step * 17.45636131) * 0.0175499250;
    color += texture(source, qt_TexCoord0 - step * 17.45636131) * 0.0175499250;
    color += texture(source, qt_TexCoord0 + step * 19.45140389) * 0.0121335220;
    color += texture(source, qt_TexCoord0 - step * 19.45140389) * 0.0121335220;
    color += texture(source, qt_TexCoord0 + step * 21.44645610) * 0.0080606440;
    color += texture(source, qt_TexCoord0 - step * 21.44645610) * 0.0080606440;
    color += texture(source, qt_TexCoord0 + step * 23.44151889) * 0.0051454546;
    color += texture(source, qt_TexCoord0 - step * 23.44151889) * 0.0051454546;
    fragColor = color;
}
