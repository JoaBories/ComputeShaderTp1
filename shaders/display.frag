#version 430 core
layout(std430, binding = 0) readonly buffer CurrentGrid { uint current[]; };
uniform uint width;
uniform uint height;
out vec4 fragColor;

void main() {
  uint x = uint(gl_FragCoord.x);
  uint y = uint(gl_FragCoord.y);
  if (x >= width || y >= height) {
    fragColor = vec4(0.0);
    return;
  }
  fragColor = vec4(vec3(1-float(current[x + y * width])), 1.0);
}