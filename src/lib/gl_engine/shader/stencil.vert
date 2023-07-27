
// [x, y, a]
layout(location = 0) in vec2 aLocation;

layout(std140) uniform Matrix {
  mat4 transform;
} uMatrix;

void main() {

  gl_Position = uMatrix.transform * vec4(aLocation, 0.0, 1.0);
}