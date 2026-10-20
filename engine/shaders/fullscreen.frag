#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1, std140) uniform UniformBuffer {
  float time;
  vec2 resolution;
} ubo;

layout(push_constant) uniform PushConstants {
  float brightness;
} pushConstants;

void main() {
  vec3 color = texture(uTexture, vUv).rgb;
  float pulse = 0.72 + 0.28 * sin(ubo.time * 1.8);
  float aspect = ubo.resolution.x / max(ubo.resolution.y, 1.0);
  vec2 centered = (vUv * 2.0 - 1.0) * vec2(aspect, 1.0);
  float vignette = 1.0 - smoothstep(0.25, 1.35, length(centered));
  outColor = vec4(color * pulse * pushConstants.brightness * vignette, 1.0);
}
