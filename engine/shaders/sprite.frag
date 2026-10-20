#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
  mat4 transform;
  vec4 tint;
} pushConstants;

void main() {
  vec4 sampled = texture(uTexture, vUv);
  outColor = vec4(sampled.rgb * pushConstants.tint.rgb, sampled.a * pushConstants.tint.a);
}
