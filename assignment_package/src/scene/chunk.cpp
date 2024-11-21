#include "chunk.h"
#include <iostream>

Chunk::Chunk(int x, int z, OpenGLContext* context) : Drawable(context), m_blocks(), minX(x), minZ(z), m_neighbors{{XPOS, nullptr}, {XNEG, nullptr}, {ZPOS, nullptr}, {ZNEG, nullptr}},
ready(false),
    loaded(false)
{
    std::fill_n(m_blocks.begin(), 65536, EMPTY);
}

// Does bounds checking with at()
BlockType Chunk::getLocalBlockAt(unsigned int x, unsigned int y, unsigned int z) {
    blockMutex.lock();
    auto p = m_blocks.at(x + 16 * y + 16 * 256 * z);
    blockMutex.unlock();
    return p;
}

// Exists to get rid of compiler warnings about int -> unsigned int implicit conversion
BlockType Chunk::getLocalBlockAt(int x, int y, int z) {
    return getLocalBlockAt(static_cast<unsigned int>(x), static_cast<unsigned int>(y), static_cast<unsigned int>(z));
}

// Does bounds checking with at()
void Chunk::setLocalBlockAt(unsigned int x, unsigned int y, unsigned int z, BlockType t) {
    blockMutex.lock();
    m_blocks.at(x + 16 * y + 16 * 256 * z) = t;
    blockMutex.unlock();
}


const static std::unordered_map<Direction, Direction, EnumHash> oppositeDirection {
    {XPOS, XNEG},
    {XNEG, XPOS},
    {YPOS, YNEG},
    {YNEG, YPOS},
    {ZPOS, ZNEG},
    {ZNEG, ZPOS}
};

void Chunk::linkNeighbor(uPtr<Chunk> &neighbor, Direction dir) {
    if(neighbor != nullptr) {
        this->m_neighbors[dir] = neighbor.get();
        neighbor->m_neighbors[oppositeDirection.at(dir)] = this;
    }
}

void Chunk::updateVBO(std::vector<glm::vec4>& interleavedData, Direction dir, const glm::vec4& pos, BlockType t, int vC, std::vector<GLuint>& indices) {
    glm::vec4 vertices[4];
    glm::vec4 color;

    switch(t) {
    case GRASS: color = glm::vec4(0.37f, 0.62f, 0.21f, 1.0f); break;
    case DIRT:  color = glm::vec4(0.47f, 0.33f, 0.23f, 1.0f); break;
    case STONE: color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f); break;
    case WATER: color = glm::vec4(0.0f, 0.0f, 0.75f, 0.7f); break;
    case SNOW: color = glm::vec4(1.0, 1.0, 1.0, 1.0f); break;
    default:    color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
    // case GRASS: color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
    // case DIRT:  color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
    // case STONE: color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
    // case WATER: color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
    // default:    color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    switch (dir) {
    case XPOS: vertices[0] = glm::vec4(1, 0, 0, 1); vertices[1] = glm::vec4(1, 1, 0, 1); vertices[2] = glm::vec4(1, 1, 1, 1); vertices[3] = glm::vec4(1, 0, 1, 1); break;
    case XNEG: vertices[0] = glm::vec4(0, 0, 0, 1); vertices[1] = glm::vec4(0, 1, 0, 1); vertices[2] = glm::vec4(0, 1, 1, 1); vertices[3] = glm::vec4(0, 0, 1, 1); break;
    case YPOS: vertices[0] = glm::vec4(0, 1, 0, 1); vertices[1] = glm::vec4(1, 1, 0, 1); vertices[2] = glm::vec4(1, 1, 1, 1); vertices[3] = glm::vec4(0, 1, 1, 1); break;
    case YNEG: vertices[0] = glm::vec4(0, 0, 0, 1); vertices[1] = glm::vec4(1, 0, 0, 1); vertices[2] = glm::vec4(1, 0, 1, 1); vertices[3] = glm::vec4(0, 0, 1, 1); break;
    case ZPOS: vertices[0] = glm::vec4(0, 0, 1, 1); vertices[1] = glm::vec4(1, 0, 1, 1); vertices[2] = glm::vec4(1, 1, 1, 1); vertices[3] = glm::vec4(0, 1, 1, 1); break;
    case ZNEG: vertices[0] = glm::vec4(0, 0, 0, 1); vertices[1] = glm::vec4(1, 0, 0, 1); vertices[2] = glm::vec4(1, 1, 0, 1); vertices[3] = glm::vec4(0, 1, 0, 1); break;
    }

    for (int i = 0; i < 4; ++i) {
        interleavedData.push_back(glm::vec4(minX, 0, minZ, 0) + pos + vertices[i]);
        interleavedData.push_back(color);
        interleavedData.push_back(glm::normalize(glm::vec4(glm::cross(glm::vec3(vertices[1] - vertices[0]), glm::vec3(vertices[2] - vertices[1])), 0)));
    }
    indices.push_back(vC + 0);
    indices.push_back(vC + 1);
    indices.push_back(vC + 2);
    indices.push_back(vC + 0);
    indices.push_back(vC + 2);
    indices.push_back(vC + 3);

}

void Chunk::generateVBOData() {
    std::vector<GLuint> indices;
    std::vector<glm::vec4> interleavedData;

    std::cout << "Generating VBO Data for " << minX << ", " << minZ << std::endl;

    int faceCount = 0;
    int vertexCount = 0;

    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 256; ++y) {
            if (y == 128) {
                // std::cout << y << std::endl;
            }
            for (int z = 0; z < 16; ++z) {
                BlockType t = getLocalBlockAt(x, y, z);
                glm::vec4 blockPos(x, y, z, 0);

                if (t != EMPTY) {
                    BlockType x_pos = (x < 15) ? getLocalBlockAt(x + 1, y, z) : (m_neighbors[XPOS] ? m_neighbors[XPOS]->getLocalBlockAt(0, y, z) : EMPTY);
                    BlockType x_neg = (x > 0) ? getLocalBlockAt(x - 1, y, z) : (m_neighbors[XNEG] ? m_neighbors[XNEG]->getLocalBlockAt(15, y, z) : EMPTY);
                    BlockType y_pos = (y < 255) ? getLocalBlockAt(x, y + 1, z) : EMPTY;
                    BlockType y_neg = (y > 0) ? getLocalBlockAt(x, y - 1, z) : EMPTY;
                    BlockType z_pos = (z < 15) ? getLocalBlockAt(x, y, z + 1) : (m_neighbors[ZPOS] ? m_neighbors[ZPOS]->getLocalBlockAt(x, y, 0) : EMPTY);
                    BlockType z_neg = (z > 0) ? getLocalBlockAt(x, y, z - 1) : (m_neighbors[ZNEG] ? m_neighbors[ZNEG]->getLocalBlockAt(x, y, 15) : EMPTY);

                    if (x_pos == EMPTY) { updateVBO(interleavedData, XPOS, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                    if (x_neg == EMPTY) { updateVBO(interleavedData, XNEG, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                    if (y_pos == EMPTY) { updateVBO(interleavedData, YPOS, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                    if (y_neg == EMPTY) { updateVBO(interleavedData, YNEG, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                    if (z_pos == EMPTY) { updateVBO(interleavedData, ZPOS, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                    if (z_neg == EMPTY) { updateVBO(interleavedData, ZNEG, blockPos, t, vertexCount, indices); faceCount++; vertexCount += 4;}
                }
            }
        }
    }
        // std::cout << "debug: face count: " << faceCount << std::endl;
        // std::cout << "debug: vertex count: " << vertexCount << std::endl;

    generateBuffer(INTERLEAVED);
    generateBuffer(INDEX);

    if (bindBuffer(INTERLEAVED)) {
        // std::cout << "debug: interleaved buffer created" << std::endl;
        mp_context->glBufferData(GL_ARRAY_BUFFER, interleavedData.size() * sizeof(glm::vec4), interleavedData.data(), GL_STATIC_DRAW);

    }

    if (bindBuffer(INDEX)) {
        // std::cout << "debug: index buffer created"  << std::endl;
        mp_context->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    }

    // std::cout << "debug: 1 index count " << this->elemCount(INDEX) << std::endl;


    if (!indices.empty()) {
        indexCounts[INDEX] = indices.size();
    } else {
        indexCounts[INDEX] = 0;
    }

    if(!interleavedData.empty()){
        indexCounts[INTERLEAVED] = interleavedData.size();
    } else {
        indexCounts[INTERLEAVED] = 0;

    }

    // std::cout << "debug: interleaved count " << this->elemCount(INTERLEAVED) << std::endl;
    // std::cout << "debug: 2 index count " << this->elemCount(INDEX) << std::endl;


}

void Chunk::createVBOdata() {
    generateVBOData();
}

void Chunk::create() {
    createVBOdata();
}
