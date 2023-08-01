
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
    } else {
        return srcColor;
    }
}

void main() {
    FragColor = blit_color();
}