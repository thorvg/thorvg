
// [x, y, a]
layout(location = 0) in vec2 aLocation;
// [u, v]
layout(location = 1) in vec2 aUV;

out vec2  vUV;

void main() {
  vUV = aUV;

  gl_Position = vec4(aLocation, 0.0, 1.0);
}