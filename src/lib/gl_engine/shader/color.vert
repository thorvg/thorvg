
// [x, y, a]
layout(location = 0) in vec3 aLocation;

uniform mat4 uTransform;

out float aOpacity;

void main() {
  aOpacity = aLocation.z;

  gl_Position = uTransform * vec4(aLocation.xy, 0.0, 1.0);
}