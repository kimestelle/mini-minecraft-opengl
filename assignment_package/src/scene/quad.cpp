#include "quad.h"

Quad::~Quad()
{}

void Quad::createVBOdata(){
    GLuint idx[6] = {0, 1, 2, 0, 2, 3};
    glm::vec4 pos[4] = {glm::vec4(-1,-1,1,1), glm::vec4(1,-1,1,1),
                        glm::vec4(1,1,1,1), glm::vec4(-1,1,1,1)};
    glm::vec4 col[4] = {glm::vec4(1, 1, 1, 1), glm::vec4(1, 1, 1, 1),
                        glm::vec4(1, 1, 1, 1), glm::vec4(1, 1, 1, 1)};
    glm::vec4 uv[4] = {glm::vec4(0, 0, 0, 0), glm::vec4(1, 0, 0, 0),
                        glm::vec4(1, 1, 0, 0), glm::vec4(0, 1, 0, 0)};
    indexCounts[OPQ_INDEX] = 6;

    generateBuffer(OPQ_INDEX);
    bindBuffer(OPQ_INDEX);
    mp_context->glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLuint), idx, GL_STATIC_DRAW);
    generateBuffer(POSITION);
    bindBuffer(POSITION);
    mp_context->glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec4), pos, GL_STATIC_DRAW);
    // generateBuffer(COLOR);
    // bindBuffer(COLOR);
    // mp_context->glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec4), col, GL_STATIC_DRAW);
    generateBuffer(UV);
    bindBuffer(UV);
    mp_context->glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(glm::vec4), uv, GL_STATIC_DRAW);
}

GLenum Quad::drawMode(){
    return GL_TRIANGLES;
}
