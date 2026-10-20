#version 450

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
  vec4 tint;
} pushConstants;

void main() {
  vec3 normal = normalize(vNormal);
  vec3 lightDirection = normalize(vec3(0.35, 0.9, 0.55));
  float diffuse = max(dot(normal, lightDirection), 0.0);
  vec3 base = texture(uTexture, vUv).rgb;
  vec3 color = base * (0.24 + 0.76 * diffuse) * pushConstants.tint.rgb;
  outColor = vec4(color, 1.0);
}
