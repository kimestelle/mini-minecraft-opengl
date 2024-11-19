#include "terrain.h"
#include "cube.h"
#include <random>
#include <stdexcept>
#include <iostream>

Terrain::Terrain(OpenGLContext *context)
    : m_chunks(), m_generatedTerrain(),
      m_chunkVBOsNeedUpdating(true), mp_context(context)
{}

Terrain::~Terrain() {
    for (auto &chunkPair : m_chunks) {
        chunkPair.second->destroyVBOdata();
    }
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
    uPtr<Chunk> chunk = mkU<Chunk>(x, z, mp_context);
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
            for (int x = minX; x < maxX; x += 16) {
                for (int z = minZ; z < maxZ; z += 16) {
                    if (hasChunkAt(x, z)) {
                        getChunkAt(x, z)->generateVBOData();
                    }
                }
            }
            m_chunkVBOsNeedUpdating = false;
        }

        for(int x = minX; x < maxX; x += 16) {
            for(int z = minZ; z < maxZ; z += 16) {
                if (hasChunkAt(x, z)) {
                    const uPtr<Chunk> &chunk = getChunkAt(x, z);
                    shaderProgram->drawOpq(*chunk);
                }
            }
        }

        for(int x = minX; x < maxX; x += 16) {
            for(int z = minZ; z < maxZ; z += 16) {
                if (hasChunkAt(x, z)) {
                    const uPtr<Chunk> &chunk = getChunkAt(x, z);
                    shaderProgram->drawTrans(*chunk);
                }
            }
        }
}
void Terrain::CreateTestScene()
{
    // TODO: DELETE THIS LINE WHEN YOU DELETE m_geomCube!

    // Create the Chunks that will
    // store the blocks for our
    // initial world space

    // for(int x = 0; x < 64; x += 16) {
    //     for(int z = 0; z < 64; z += 16) {
            instantiateChunkAt(0, 0);
    //     }
    // }
    // Tell our existing terrain set that
    // the "generated terrain zone" at (0,0)
    // now exists.
    m_generatedTerrain.insert(toKey(0, 0));

    // Create the basic terrain floor
    // for(int x = 0; x < 64; ++x) {
    //     for(int z = 0; z < 64; ++z) {
    //         if((x + z) % 2 == 0) {
    //             setGlobalBlockAt(x, 128, z, STONE);
    //         }
    //         else {
    //             setGlobalBlockAt(x, 128, z, DIRT);
    //         }
    //     }
    // }
    // // Add "walls" for collision testing
    // for(int x = 0; x < 64; ++x) {
    //     setGlobalBlockAt(x, 129, 16, GRASS);
    //     setGlobalBlockAt(x, 130, 16, GRASS);
    //     setGlobalBlockAt(x, 129, 48, GRASS);
    //     setGlobalBlockAt(16, 130, x, GRASS);
    // }
    // // Add a central column
    // for(int y = 129; y < 140; ++y) {
    //     setGlobalBlockAt(32, y, 32, GRASS);
    // }

    setGlobalBlockAt(0, 128, 0, GRASS);

    //create vbo data
    for(int x = 0; x < 64; x += 16) {
        for(int z = 0; z < 64; z += 16) {
            if (hasChunkAt(x, z)) {
                getChunkAt(x, z)->createVBOdata();
            }
        }
    }


}

//function to expand terrain based on player position
void Terrain::expandTerrainIfNeeded(const glm::vec3 &playerPos) {
    int xCenter = ((int)playerPos.x) - ((int)playerPos.x % 16);
    int zCenter = ((int)playerPos.z) - ((int)playerPos.z % 16);

    for(int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if(!hasChunkAt(xCenter + i * 16, zCenter + j * 16)) {
                // std::cout << "expanding Terrain" << std::endl;
                instantiateChunkAt(xCenter + i * 16, zCenter + j * 16);
            }
        }
    }

    xCenter = ((int)playerPos.x) - ((int)playerPos.x % 64);
    zCenter = ((int)playerPos.z) - ((int)playerPos.z % 64);

    for(int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            int c = m_generatedTerrain.count(toKey(xCenter + 64*i, zCenter + 64 * j));
            if (c == 0) {
                // std::cout << "expanding generation" << std::endl;
                m_generatedTerrain.insert(toKey(xCenter + 64*i, zCenter + 64 * j));
            }
        }
    }
}


float PerlinNoise(float x, float y);


void Terrain::GenerateTerrain(int xPos, int zPos)  {

    if(m_generatedTerrain.count(toKey(xPos, zPos)) > 0) {
        return;
    }

    int WinChunks = 4;

    // Create the Chunks that will
    // store the blocks for our
    // initial world space
    for(int x = xPos; x < 16*WinChunks + xPos; x += 16) {
        for(int z = zPos; z < 16*WinChunks + zPos; z += 16) {
            instantiateChunkAt(x, z);
        }
    }
    // Tell our existing terrain set that
    // the "generated terrain zone" at (0,0)
    // now exists.
    m_generatedTerrain.insert(toKey(xPos, zPos));

    // Create the basic terrain floor
    for(int y = 0; y < 256; y++) {
        for (int x = xPos; x < 16*WinChunks + xPos; x++) {
            for (int z = zPos; z < 16*WinChunks + zPos; z++) {

                float noise = PerlinNoise(0.05 * x * 0.6237f, 0.05 * z * 0.6237f);

                float typeTerrain = PerlinNoise(x * 0.01f * 1.5346 + 0.3246, z * 0.01f * 1.5346 + 0.8756);

                if (y <= 128) {
                    setGlobalBlockAt(x, y, z, GRASS);
                } else if (y == 129) {
                    if (typeTerrain < 0.5) {
                        if(noise >= (0.3)) {
                            setGlobalBlockAt(x, y, z, GRASS);
                        }
                    } else {
                        if(noise >= (0.3)) {
                            setGlobalBlockAt(x, y, z, STONE);
                        }
                    }
                }
                else {
                    float rand;
                    if (typeTerrain < 0.5) {
                        rand = noise - 0.06 * (y - 130);

                        if(rand > 0.35f) {
                            setGlobalBlockAt(x, y, z, GRASS);
                        }
                    } else {
                        rand = (noise * glm::min(2.0f, 1.0f)) - 0.03 * (y - 130);
                        // rand *= typeTerrain;

                        if(rand > 0.35f) {
                            setGlobalBlockAt(x, y, z, STONE);
                        }
                    }
                }

                if(getGlobalBlockAt(x, y, z) == EMPTY && y < 130 && y >= 125) {
                    setGlobalBlockAt(x, y, z, WATER);
                }
            }
        }

        std::vector<std::vector<bool>> newPoints(16*WinChunks);

        for(int i = 0; i < newPoints.size(); i++) {
            newPoints[i] = std::vector<bool>(16*WinChunks);
        }

        for (int i = xPos; i < 16*WinChunks + xPos; i++) {
            for (int j = zPos; j < 16*WinChunks + zPos; j++) {
                int numNeighbors = 0;

                if (getGlobalBlockAt(i, y, j) == EMPTY && i > 0 && i < 16*WinChunks - 1 && j > 0 && j < 16*WinChunks - 1) {

                    numNeighbors += getGlobalBlockAt(i, y, j-1) != EMPTY ? 1 : 0;
                    numNeighbors += getGlobalBlockAt(i, y, j+1) != EMPTY ? 1 : 0;
                    numNeighbors += getGlobalBlockAt(i-1, y, j) != EMPTY ? 1 : 0;
                    numNeighbors += getGlobalBlockAt(i+1, y, j) != EMPTY ? 1 : 0;

                    float probability = 0.235f * numNeighbors;

                    float first = PerlinNoise(i * 0.20f * 12.3358f + 2543537.6523f, j * 0.20f * 13.654f + 544523.3644f);

                    if (first < probability) {
                        newPoints[i - xPos][j - zPos] = true;
                    }
                }
            }
        }

        for (int i = xPos; i < 16*WinChunks + xPos; i++) {
            for (int j = zPos; j < 16*WinChunks + zPos; j++) {
                if (newPoints[i - xPos][j - zPos]) {
                    setGlobalBlockAt(i, y, j, getGlobalBlockAt(i-1, y, j));
                }
            }
        }
    }


    for(int y = 0; y < 255; y++) {
        for (int x = xPos; x < 16*WinChunks + xPos; x++) {
            for (int z = zPos; z < 16*WinChunks + zPos; z++) {
                if(getGlobalBlockAt(x, y, z) == EMPTY) {
                    continue;
                } else if (getGlobalBlockAt(x, y, z) == GRASS) {
                    if(getGlobalBlockAt(x, y+1, z) == EMPTY|| getGlobalBlockAt(x, y+1, z) == WATER || y == 255) {
                        setGlobalBlockAt(x, y, z, GRASS);
                    }
                    else {
                        setGlobalBlockAt(x, y, z, STONE);
                    }
                } else if (getGlobalBlockAt(x, y, z) == STONE) {
                    if(getGlobalBlockAt(x, y+1, z) == EMPTY || getGlobalBlockAt(x, y+1, z) == WATER || y == 255) {
                        setGlobalBlockAt(x, y, z, SNOW);
                    }
                    else {
                        setGlobalBlockAt(x, y, z, STONE);
                    }
                }
            }
        }
    }

    for(int x = xPos; x < 16*WinChunks + xPos; x += 16) {
        for(int z = zPos; z < 16*WinChunks + zPos; z += 16) {
            getChunkAt(x, z)->createVBOdata();
        }
    }
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







