#version 450
// layout(constant_id = 0) const uint TextureCount = 64U;
const uint TextureCount = 64U;

layout(set = 0, binding = 2) uniform sampler2D textures[TextureCount];

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint tex;
layout(location = 2) in vec4 vertexColor;

layout(location = 0) out vec4 fragmentColor;

void main() {
    fragmentColor = vertexColor * (tex > 0 ? texture(textures[tex], uv) : vec4(1.0));
}
