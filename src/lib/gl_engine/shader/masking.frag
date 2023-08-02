
// compose method defined in thorvg.h
#define NONE                0
#define ALPHA_MASK          2
#define INV_ALPHA_MASK      3
#define LUMA_MASK           4
#define INV_LUMA_MASK       5
#define ADD_MASK            6
#define SUB_MASK            7
#define INTERSECT           8
#define DIFF_MASK           9

layout(std140) uniform MaskInfo {
    int method;
    int opacity;
    int dum1;
    int dum2;
} uMaskInfo;

uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;

out vec4 FragColor;

vec4 blit_color() {
    vec4 srcColor = texture(uSrcTexture, vUV);
    vec4 dstColor = texture(uDstTexture, vUV);

    float alpha = float(uMaskInfo.opacity) / 255.0;

    if (uMaskInfo.method == NONE) {
        return dstColor * alpha;
    } else if (uMaskInfo.method == ALPHA_MASK) {
        return srcColor * dstColor.a * alpha;
    } else if (uMaskInfo.method == INV_ALPHA_MASK) {
        return srcColor * (1.0 - dstColor.a) * alpha;
    } else if (uMaskInfo.method == LUMA_MASK) {
        return srcColor * alpha * (0.299 * dstColor.r + 0.587 * dstColor.g + 0.114 * dstColor.b);
    } else if (uMaskInfo.method == INV_LUMA_MASK) {
        float luma = (0.299 * dstColor.r + 0.587 * dstColor.g + 0.114 * dstColor.b);
        return srcColor * alpha * (1.0 - luma);
    } else if (uMaskInfo.method == ADD_MASK) {
        vec4 color = srcColor + dstColor * (1.0 - srcColor.a);
        return min(color, vec4(1.0, 1.0, 1.0, 1.0));
    } else if (uMaskInfo.method == SUB_MASK) {
        return dstColor * alpha * (abs(dstColor.a - srcColor.a));
    } else if (uMaskInfo.method == INTERSECT) {
        float intAlpha = srcColor.a * dstColor.a;
        return dstColor * alpha * intAlpha;
    } else if (uMaskInfo.method == DIFF_MASK) {
        return abs(srcColor - dstColor);
    } else {
        return srcColor;
    }
}

void main() {
    FragColor = blit_color();
}