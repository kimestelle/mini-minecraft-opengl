#pragma once
#include "smartpointerhelp.h"
#include "glm_includes.h"
#include <array>
#include <mutex>
#include <unordered_map>
#include <cstddef>

#include <drawable.h>


//using namespace std;

// C++ 11 allows us to define the size of an enum. This lets us use only one byte
// of memory to store our different block types. By default, the size of a C++ enum
// is that of an int (so, usually four bytes). This *does* limit us to only 256 different
// block types, but in the scope of this project we'll never get anywhere near that many.
enum BlockType : unsigned char
{
    EMPTY, GRASS, DIRT, STONE, WATER, SNOW, LAVA, BEDROCK, SAND
};

// The six cardinal directions in 3D space
enum Direction : unsigned char
{
    XPOS, XNEG, YPOS, YNEG, ZPOS, ZNEG
};

// Lets us use any enum class as the key of a
// std::unordered_map
struct EnumHash {
    template <typename T>
    size_t operator()(T t) const {
        return static_cast<size_t>(t);
    }
};

// One Chunk is a 16 x 256 x 16 section of the world,
// containing all the Minecraft blocks in that area.
// We divide the world into Chunks in order to make
// recomputing its VBO data faster by not having to
// render all the world at once, while also not having
// to render the world block by block.

// TODO have Chunk inherit from Drawable
class Chunk : public Drawable {
private:
    // All of the blocks contained within this Chunk
    std::array<BlockType, 65536> m_blocks;
    // The coordinates of the chunk's lower-left corner in world space
    int minX, minZ;
    // This Chunk's four neighbors to the north, south, east, and west
    // The third input to this map just lets us use a Direction as
    // a key for this map.
    // These allow us to properly determine
    std::unordered_map<Direction, Chunk*, EnumHash> m_neighbors;
    std::mutex blockMutex;



    std::vector<GLuint> opq_indices;
    std::vector<glm::vec4> opq_interleavedData;

    std::vector<GLuint> trans_indices;
    std::vector<glm::vec4> trans_interleavedData;

public:
    bool ready;
    bool loaded;
    bool working;

    Chunk(int x, int z, OpenGLContext* context);
    static std::unordered_map<BlockType, glm::vec2> blockUVs;
    static glm::vec2 getUV(BlockType t, Direction dir);

    void create();
    void generateVBOData();
    void loadVBO();
    void loadToGPU();
    void updateVBO(std::vector<glm::vec4>& interleavedData, Direction dir, const glm::vec4& pos, BlockType t, int vC, std::vector<GLuint>& indices);

    void createVBOdata() override;
    GLenum drawMode() override { return GL_TRIANGLES; }

    BlockType getLocalBlockAt(unsigned int x, unsigned int y, unsigned int z);
    BlockType getLocalBlockAt(int x, int y, int z) ;
    void setLocalBlockAt(unsigned int x, unsigned int y, unsigned int z, BlockType t);
    void linkNeighbor(uPtr<Chunk>& neighbor, Direction dir);
};
