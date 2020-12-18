#version 450

// Matches StepMania::Rect<float>
struct Rect {
    float left;
    float top;
    float right;
    float bottom;
};

// Matches QuadProgram::Quad
struct Quad {
    Rect rect;
    vec4 color[4];
    Rect tex;
};

layout(std430, binding = 0) buffer Quads {
    Quad quads[];
};

layout(location = 0) out vec4 vertexColor;

void main() {
    int index = gl_VertexIndex >> 2;
    int corner = gl_VertexIndex & 3;
    vec2 pos;
    switch (corner) {
        case 0: pos = vec2(quads[index].rect.left, quads[index].rect.top); break;
        case 1: pos = vec2(quads[index].rect.left, quads[index].rect.bottom); break;
        case 2: pos = vec2(quads[index].rect.right, quads[index].rect.bottom); break;
        case 3: pos = vec2(quads[index].rect.right, quads[index].rect.top); break;
    }
    gl_Position = vec4(pos, 0.0, 1.0);
    vertexColor = quads[index].color[corner];
}
