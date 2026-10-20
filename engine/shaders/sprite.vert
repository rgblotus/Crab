#version 450

layout(location = 0) out vec2 vUv;

layout(push_constant) uniform PushConstants {
  mat4 transform;
  vec4 tint;
} pushConstants;

void main() {
  const vec2 positions[6] = vec2[](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
  );
  const vec2 uvs[6] = vec2[](
    vec2(0.0, 1.0), vec2(1.0, 1.0), vec2(1.0, 0.0),
    vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(0.0, 0.0)
  );

  vec2 position = positions[gl_VertexIndex];
  vUv = uvs[gl_VertexIndex];
  gl_Position = pushConstants.transform * vec4(position, 0.0, 1.0);
}
