
precision highp float;

const int MAX_STOP_COUNT = 4;


layout(std140) uniform GradientInfo {
  int nStops;int dum;int dum1;int dum2;
  vec2  centerPos;
  vec2  radius;
  vec4  stopPoints;
  vec4  stopColors[MAX_STOP_COUNT];
} uGradientInfo ;


in float aOpacity;
in vec2 aPos;


out vec4 FragColor;


float gradientStep(float edge0, float edge1, float x) {
  x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);

  return x;
}

vec4 gradient(float t) {
  vec4 col = vec4(0.0);
  int i = 0;
  int count = uGradientInfo.nStops;

  if (t <= uGradientInfo.stopPoints[0]) {
    col += uGradientInfo.stopColors[0];
  } else if (t >= uGradientInfo.stopPoints[count - 1]) {
    col += uGradientInfo.stopColors[count - 1];
  } else {
    for(i = 0; i < count - 1; i++) {
      if (t > uGradientInfo.stopPoints[i] && t < uGradientInfo.stopPoints[i + 1]) {
        col += (uGradientInfo.stopColors[i] * (1.0 - gradientStep(uGradientInfo.stopPoints[i], uGradientInfo.stopPoints[i + 1], t)));
        col += (uGradientInfo.stopColors[i + 1] * gradientStep(uGradientInfo.stopPoints[i], uGradientInfo.stopPoints[i + 1], t));
      }
    }
  }

  return col;
}

void main() {
  vec2 pos = aPos;

  float d = distance(pos, uGradientInfo.centerPos);

  float t = d / uGradientInfo.radius[0];

  t = clamp(t, 0.0, 1.0);

  vec4 color = gradient(t);

  // convert to premultiplied alpha
  FragColor = vec4(color.rgb * color.a, color.a);
}