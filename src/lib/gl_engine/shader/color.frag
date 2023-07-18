
uniform vec4 uColor;

in float aOpacity;

out vec4 FragColor;

void main() {
    // in generate we need premultiplied alpha color
    FragColor = vec4(uColor.xyz * uColor.w * aOpacity, uColor.w * aOpacity);
}