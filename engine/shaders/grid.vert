#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 1, std140) uniform UniformBuffer {
  float time;
  vec2 resolution;
  mat4 viewProjection;
  mat4 model;
} ubo;

void main() {
  gl_Position = ubo.viewProjection * vec4(inPosition, 1.0);
}
