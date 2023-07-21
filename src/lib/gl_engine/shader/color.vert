
// [x, y, a]
layout(location = 0) in vec3 aLocation;

layout(std140) uniform Matrix {
  mat4 transform;
} uMatrix;

out float aOpacity;

void main() {
  aOpacity = aLocation.z;

  gl_Position = uMatrix.transform * vec4(aLocation.xy, 0.0, 1.0);
}