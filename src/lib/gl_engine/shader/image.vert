
// [x, y, a]
layout(location = 0) in vec3 aLocation;
// [u, v]
layout(location = 1) in vec2 aUV;

layout(std140) uniform Matrix {
  mat4 transform;
} uMatrix;

out float aOpacity;
out vec2  vUV;

void main() {
  aOpacity = aLocation.z;
  vUV = aUV;

  gl_Position = uMatrix.transform * vec4(aLocation.xy, 0.0, 1.0);
}