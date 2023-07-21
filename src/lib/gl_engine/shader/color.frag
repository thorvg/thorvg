
uniform vec4 uColor;

layout(std140) uniform ColorInfo {
    vec4 solidColor;
} uColorInfo;

in float aOpacity;

out vec4 FragColor;

void main() {
    vec4 uColor = uColorInfo.solidColor;
    // in generate we need premultiplied alpha color
    FragColor = vec4(uColor.xyz * uColor.w * aOpacity, uColor.w * aOpacity);
}