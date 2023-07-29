
// compose method defined in thorvg.h
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
} uMaskInfo;

uniform sampler2D uSrcTexture;
uniform sampler2D uDstTexture;

in vec2 vUV;

out vec4 FragColor;


void main() {
    vec4 color = texture(uSrcTexture, vUV);

    FragColor = color;
}