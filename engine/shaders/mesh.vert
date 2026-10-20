#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUv;

layout(set = 0, binding = 1, std140) uniform UniformBuffer {
  float time;
  vec2 resolution;
  mat4 viewProjection;
  mat4 model;
} ubo;

void main() {
  gl_Position = ubo.viewProjection * ubo.model * vec4(inPosition, 1.0);
  vNormal = mat3(ubo.model) * inNormal;
  vUv = inUv;
}
