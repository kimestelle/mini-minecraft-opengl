#include "terrain.h"
#include "cube.h"
#include <random>
#include <stdexcept>
#include <iostream>

Terrain::Terrain(OpenGLContext *context)
    : m_chunks(), m_generatedTerrain(), m_geomCube(context),
      m_chunkVBOsNeedUpdating(true), mp_context(context)
{}

Terrain::~Terrain() {
    m_geomCube.destroyVBOdata();
}

// Combine two 32-bit ints into one 64-bit int
// where the upper 32 bits are X and the lower 32 bits are Z
int64_t toKey(int x, int z) {
    int64_t xz = 0xffffffffffffffff;
    int64_t x64 = x;
    int64_t z64 = z;

    // Set all lower 32 bits to 1 so we can & with Z later
    xz = (xz & (x64 << 32)) | 0x00000000ffffffff;

    // Set all upper 32 bits to 1 so we can & with XZ
    z64 = z64 | 0xffffffff00000000;

    // Combine
    xz = xz & z64;
    return xz;
}

glm::ivec2 toCoords(int64_t k) {
    // Z is lower 32 bits
    int64_t z = k & 0x00000000ffffffff;
    // If the most significant bit of Z is 1, then it's a negative number
    // so we have to set all the upper 32 bits to 1.
    // Note the 8    V
    if(z & 0x0000000080000000) {
        z = z | 0xffffffff00000000;
    }
    int64_t x = (k >> 32);

    return glm::ivec2(x, z);
}

// Surround calls to this with try-catch if you don't know whether
// the coordinates at x, y, z have a corresponding Chunk
BlockType Terrain::getGlobalBlockAt(int x, int y, int z) const
{
    if(hasChunkAt(x, z)) {
        // Just disallow action below or above min/max height,
        // but don't crash the game over it.
        if(y < 0 || y >= 256) {
            return EMPTY;
        }
        const uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        return c->getLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                                  static_cast<unsigned int>(y),
                                  static_cast<unsigned int>(z - chunkOrigin.y));
    }
    else {
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

BlockType Terrain::getGlobalBlockAt(glm::vec3 p) const {
    return getGlobalBlockAt(p.x, p.y, p.z);
}

bool Terrain::hasChunkAt(int x, int z) const {
    // Map x and z to their nearest Chunk corner
    // By flooring x and z, then multiplying by 16,
    // we clamp (x, z) to its nearest Chunk-space corner,
    // then scale back to a world-space location.
    // Note that floor() lets us handle negative numbers
    // correctly, as floor(-1 / 16.f) gives us -1, as
    // opposed to (int)(-1 / 16.f) giving us 0 (incorrect!).
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks.find(toKey(16 * xFloor, 16 * zFloor)) != m_chunks.end();
}


uPtr<Chunk>& Terrain::getChunkAt(int x, int z) {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks[toKey(16 * xFloor, 16 * zFloor)];
}


const uPtr<Chunk>& Terrain::getChunkAt(int x, int z) const {
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks.at(toKey(16 * xFloor, 16 * zFloor));
}

void Terrain::setGlobalBlockAt(int x, int y, int z, BlockType t)
{
    if(hasChunkAt(x, z)) {
        uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        c->setLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                           static_cast<unsigned int>(y),
                           static_cast<unsigned int>(z - chunkOrigin.y),
                           t);
        m_chunkVBOsNeedUpdating = true;
    }
    else {
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

Chunk* Terrain::instantiateChunkAt(int x, int z) {
    uPtr<Chunk> chunk = mkU<Chunk>(x, z);
    Chunk *cPtr = chunk.get();
    m_chunks[toKey(x, z)] = move(chunk);
    // Set the neighbor pointers of itself and its neighbors
    if(hasChunkAt(x, z + 16)) {
        auto &chunkNorth = m_chunks[toKey(x, z + 16)];
        cPtr->linkNeighbor(chunkNorth, ZPOS);
    }
    if(hasChunkAt(x, z - 16)) {
        auto &chunkSouth = m_chunks[toKey(x, z - 16)];
        cPtr->linkNeighbor(chunkSouth, ZNEG);
    }
    if(hasChunkAt(x + 16, z)) {
        auto &chunkEast = m_chunks[toKey(x + 16, z)];
        cPtr->linkNeighbor(chunkEast, XPOS);
    }
    if(hasChunkAt(x - 16, z)) {
        auto &chunkWest = m_chunks[toKey(x - 16, z)];
        cPtr->linkNeighbor(chunkWest, XNEG);
    }
    return cPtr;
    return cPtr;
}

// TODO: When you make Chunk inherit from Drawable, change this code so
// it draws each Chunk with the given ShaderProgram
void Terrain::draw(int minX, int maxX, int minZ, int maxZ, ShaderProgram *shaderProgram) {

    if(m_chunkVBOsNeedUpdating) {
        m_geomCube.clearOffsetBuf();
        m_geomCube.clearColorBuf();

        std::vector<glm::vec3> offsets, colors;

        for(int x = minX; x < maxX; x += 16) {
            for(int z = minZ; z < maxZ; z += 16) {
                const uPtr<Chunk> &chunk = getChunkAt(x, z);
                for(int i = 0; i < 16; ++i) {
                    for(int j = 0; j < 256; ++j) {
                        for(int k = 0; k < 16; ++k) {
                            BlockType t = chunk->getLocalBlockAt(i, j, k);

                            if(t != EMPTY) {
                                offsets.push_back(glm::vec3(i+x, j, k+z));
                                switch(t) {
                                case GRASS:
                                    colors.push_back(glm::vec3(95.f, 159.f, 53.f) / 255.f);
                                    break;
                                case DIRT:
                                    colors.push_back(glm::vec3(121.f, 85.f, 58.f) / 255.f);
                                    break;
                                case STONE:
                                    colors.push_back(glm::vec3(0.5f));
                                    break;
                                case WATER:
                                    colors.push_back(glm::vec3(0.f, 0.f, 0.75f));
                                    break;
                                default:
                                    // Other block types are not yet handled, so we default to debug purple
                                    colors.push_back(glm::vec3(1.f, 0.f, 1.f));
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        m_geomCube.createInstancedVBOdata(offsets, colors);
        m_chunkVBOsNeedUpdating = false;
    }
    shaderProgram->drawInstanced(m_geomCube);
}

void Terrain::CreateTestScene()
{
    // TODO: DELETE THIS LINE WHEN YOU DELETE m_geomCube!
    m_geomCube.createVBOdata();

    // Create the Chunks that will
    // store the blocks for our
    // initial world space
    for(int x = 0; x < 64; x += 16) {
        for(int z = 0; z < 64; z += 16) {
            instantiateChunkAt(x, z);
        }
    }
    // Tell our existing terrain set that
    // the "generated terrain zone" at (0,0)
    // now exists.
    m_generatedTerrain.insert(toKey(0, 0));

    // Create the basic terrain floor
    for(int x = 0; x < 64; ++x) {
        for(int z = 0; z < 64; ++z) {
            if((x + z) % 2 == 0) {
                setGlobalBlockAt(x, 128, z, STONE);
            }
            else {
                setGlobalBlockAt(x, 128, z, DIRT);
            }
        }
    }
    // Add "walls" for collision testing
    for(int x = 0; x < 64; ++x) {
        setGlobalBlockAt(x, 129, 16, GRASS);
        setGlobalBlockAt(x, 130, 16, GRASS);
        setGlobalBlockAt(x, 129, 48, GRASS);
        setGlobalBlockAt(16, 130, x, GRASS);
    }
    // Add a central column
    for(int y = 129; y < 140; ++y) {
        setGlobalBlockAt(32, y, 32, GRASS);
    }
}


float PerlinNoise(float x, float y);


void Terrain::GenerateTerrain()  {
    // TODO: DELETE THIS LINE WHEN YOU DELETE m_geomCube!
    m_geomCube.createVBOdata();

    // Create the Chunks that will
    // store the blocks for our
    // initial world space
    for(int x = 0; x < 64; x += 16) {
        for(int z = 0; z < 64; z += 16) {
            instantiateChunkAt(x, z);
        }
    }
    // Tell our existing terrain set that
    // the "generated terrain zone" at (0,0)
    // now exists.
    m_generatedTerrain.insert(toKey(0, 0));

    setGlobalBlockAt(0, 128, 0, GRASS);


    // // Create the basic terrain floor
    // for(int y = 0; y < 256; y++) {
    //     for (int x = 0; x < 64; x++) {
    //         for (int z = 0; z < 64; z++) {
    //             float noise = PerlinNoise(0.25 * x * 0.6237f, 0.25 * z * 0.5663f);

    //             if (y <= 128) {
    //                 setGlobalBlockAt(x, y, z, GRASS);
    //             } else if (y == 129) {
    //                 if(noise >= (0.2 + (y - 128) * 0.1)) {
    //                     setGlobalBlockAt(x, y, z, GRASS);
    //                 }
    //             }
    //             else {
    //                 if(getGlobalBlockAt(x, y-1, z) != EMPTY) {
    //                     float rand = PerlinNoise(x * 0.15f  * 0.6678f + 537.6523f, (y - 128) * 0.15f * 0.6734f + 5272.545f);

    //                     rand -= glm::max(noise * 0.5f, 0.0f);

    //                     if (rand < 0.8 - 0.1f * (y - 128)) {
    //                         setGlobalBlockAt(x, y, z, GRASS);
    //                     }
    //                 }
    //             }
    //         }
    //     }

    //     std::vector<std::vector<bool>> newPoints(64);

    //     for(int i = 0; i < newPoints.size(); i++) {
    //         newPoints[i] = std::vector<bool>(64);
    //     }

    //     for (int i = 0; i < 64; i++) {
    //         for (int j = 0; j < 64; j++) {
    //             int numNeighbors = 0;

    //             if (getGlobalBlockAt(i, y, j) == EMPTY && i > 0 && i < 63 && j > 0 && j < 63) {

    //                 numNeighbors += getGlobalBlockAt(i, y, j-1) != EMPTY ? 1 : 0;
    //                 numNeighbors += getGlobalBlockAt(i, y, j+1) != EMPTY ? 1 : 0;
    //                 numNeighbors += getGlobalBlockAt(i-1, y, j) != EMPTY ? 1 : 0;
    //                 numNeighbors += getGlobalBlockAt(i+1, y, j) != EMPTY ? 1 : 0;

    //                 float probability = 0.235f * numNeighbors;

    //                 float first = PerlinNoise(i * 0.15f * 12.3358f + 2543537.6523f, j * 0.15f * 13.654f + 544523.3644f);

    //                 if (first < probability) {
    //                     newPoints[i][j] = true;
    //                 }
    //             }
    //         }
    //     }

    //     for (int i = 0; i < 64; i++) {
    //         for (int j = 0; j < 64; j++) {
    //             if (newPoints[i][j]) {
    //                 setGlobalBlockAt(i, y, j, GRASS);
    //             }
    //         }
    //     }
    // }


    // for(int y = 0; y < 255; y++) {
    //     for (int x = 0; x < 64; x++) {
    //         for (int z = 0; z < 64; z++) {
    //             if(getGlobalBlockAt(x, y, z) == EMPTY) {
    //                 continue;
    //             }

    //             if(getGlobalBlockAt(x, y+1, z) == EMPTY || y == 255) {
    //                 setGlobalBlockAt(x, y, z, GRASS);
    //             } else {
    //                 setGlobalBlockAt(x, y, z, STONE);
    //             }
    //         }
    //     }
    // }
}



float PerlinNoise(float x, float y) {
    // Fade function to smooth the interpolation
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };

    // Gradient function (returns a pseudo-random gradient value)
    auto grad = [](int hash, float x, float y) -> float {
        int h = hash & 15;
        float u = (h < 8) ? x : y;
        float v = (h < 4) ? y : (h == 12 || h == 14) ? x : 0;
        return (h & 1 ? -u : u) + (h & 2 ? -v : v);
    };

    // Permutation table (fixed, deterministic)
    static const int p[] = {
        151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96, 53, 194, 233, 7, 225,
        140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23, 190, 6, 148,
        247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32,
        57, 177, 33, 88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175,
        74, 165, 71, 134, 139, 48, 27, 166, 77, 146, 158, 231, 83, 109, 32, 41,
        131, 94, 255, 125, 48, 167, 57, 183, 146, 103, 190, 38, 127, 190, 115, 103
    };

    // Hash function to map 2D coordinates to a deterministic pseudo-random value
    auto hash = [&](int x, int y) -> int {
        return p[(x + p[(y & 255)]) & 255];
    };

    // Determine grid cell coordinates
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    // Relative coordinates in grid cell
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);

    // Fade curves for smooth interpolation
    float u = fade(xf);
    float v = fade(yf);

    // Hash coordinates of the 4 corners of the unit square
    int aa = hash(X, Y);
    int ab = hash(X, Y + 1);
    int ba = hash(X + 1, Y);
    int bb = hash(X + 1, Y + 1);

    // Interpolate gradients using fade function
    float gradAA = grad(aa, xf, yf);
    float gradAB = grad(ab, xf, yf - 1);
    float gradBA = grad(ba, xf - 1, yf);
    float gradBB = grad(bb, xf - 1, yf - 1);

    // Interpolate the results
    float x1 = glm::mix(gradAA, gradBA, u);
    float x2 = glm::mix(gradAB, gradBB, u);
    return glm::mix(x1, x2, v) * 0.5f + 0.5f; // Normalize the result to [0, 1]
}





// terrain functions







