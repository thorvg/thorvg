precision highp float;

#define FMT_ABGR8888  0
#define FMT_ARGB8888  1
#define FMT_ABGR8888S 2
#define FMT_ARGB8888S 3

layout(std140) uniform ColorInfo {
    int format;
    int flipY;
    int opacity;
    int dum2;
} uColorInfo;

uniform sampler2D uTexture;

in float aOpacity;
in vec2 vUV;

out vec4 FragColor;

void main() {

    vec2 uv = vUV;

    if (uColorInfo.flipY == 1) {
        uv.y = 1.0 - uv.y;
    }

    vec4 color = texture(uTexture, uv);
    
    vec4 result;

    if (uColorInfo.format == FMT_ABGR8888) {
         result = color;   
    } else if (uColorInfo.format == FMT_ARGB8888) {
        result.r = color.b;
        result.g = color.g;
        result.b = color.r;
        result.a = color.a;
    } else if (uColorInfo.format == FMT_ABGR8888S) {
        result = vec4(color.rgb * color.a, color.a);
    } else {
        result.r = color.b * color.a;
        result.g = color.g * color.a;
        result.b = color.r * color.a;
        result.a = color.a * color.a;
    }

    float opacity = float(uColorInfo.opacity) / 255.0;


    FragColor = vec4(result.rgb * aOpacity * opacity, result.a * aOpacity * opacity);
}