#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
  vec4 tint;
} pushConstants;

void main() {
  outColor = pushConstants.tint;
}
