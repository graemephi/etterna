#version 450

// Matches StepMania::Rect<float>
struct Rect {
    float left;
    float top;
    float right;
    float bottom;
};

// Not a vec4 so we can pack Quads
struct Color {
    float r, g, b, a;
};

vec4 color_to_vec(Color c)
{
    return vec4(c.r, c.g, c.b, c.a);
}

// Matches QuadProgram::Quad
struct Quad {
    Rect rect;
    Rect tex;
    Color colors[4];
    uint matrixIndices;
    uint textureIndex;
};

layout(std430, set = 0, binding = 0) readonly buffer Quads {
    Quad quads[];
};

layout(set = 0, binding = 1) readonly uniform Matrices {
    mat4x4 matrices[1024];
};

layout(location = 0) out vec2 uv;
layout(location = 1) out uint texture;
layout(location = 2) out vec4 vertexColor;

void main() {
    uint index = gl_VertexIndex >> 2;
    uint corner = gl_VertexIndex & 3;
    Quad quad = quads[index];
    vec2 pos;
    switch (corner) {
        case 0: {
            pos = vec2(quad.rect.left, quad.rect.top);
            uv = vec2(quad.tex.left, quad.tex.top);
        } break;
        case 1: {
            pos = vec2(quad.rect.left, quad.rect.bottom);
            uv = vec2(quad.tex.left, quad.tex.bottom);
        } break;
        case 2: {
            pos = vec2(quad.rect.right, quad.rect.bottom);
            uv = vec2(quad.tex.right, quad.tex.bottom);
        } break;
        case 3: {
            pos = vec2(quad.rect.right, quad.rect.top);
            uv = vec2(quad.tex.right, quad.tex.top);
        } break;
    }
    mat4x4 world = matrices[quad.matrixIndices & 0xffff];
    mat4x4 view = matrices[(quad.matrixIndices >> 16) & 0xff];
    mat4x4 proj = matrices[quad.matrixIndices >> 24];
    gl_Position = proj * (view * (world * vec4(pos, 0.0, 1.0)));

    vertexColor = color_to_vec(quad.colors[corner]);
    texture = quad.textureIndex;
}