#include "terrain.h"
#include "cube.h"
#include <random>
#include <stdexcept>
#include <iostream>
#include <thread>

Terrain::Terrain(OpenGLContext *context)
    : m_chunks(), m_generatedTerrain(),
      chunkMutex(),
        mp_context(context)
{}

Terrain::~Terrain(){
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
BlockType Terrain::getGlobalBlockAt(int x, int y, int z)
{
    chunkMutex.lock();
    if(hasChunkAt(x, z)) {
        // Just disallow action below or above min/max height,
        // but don't crash the game over it.
        if(y < 0 || y >= 256) {
            return EMPTY;
        }
        const uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        auto temp = c->getLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                                       static_cast<unsigned int>(y),
                                       static_cast<unsigned int>(z - chunkOrigin.y));
        chunkMutex.unlock();
        return temp;
    }
    else {
        chunkMutex.unlock();
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                    std::to_string(z) + " have no Chunk!");
    }
}

BlockType Terrain::getGlobalBlockAt(glm::vec3 p) {
    return getGlobalBlockAt(p.x, p.y, p.z);
}

bool Terrain::hasChunkAt(int x, int z) {
    // Map x and z to their nearest Chunk corner
    // By flooring x and z, then multiplying by 16,
    // we clamp (x, z) to its nearest Chunk-space corner,
    // then scale back to a world-space location.
    // Note that floor() lets us handle negative numbers
    // correctly, as floor(-1 / 16.f) gives us -1, as
    // opposed to (int)(-1 / 16.f) giving us 0 (incorrect!).
    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    auto p = m_chunks.find(toKey(16 * xFloor, 16 * zFloor)) != m_chunks.end();
    return p;
}


uPtr<Chunk>& Terrain::getChunkAt(int x, int z) {

    int xFloor = static_cast<int>(glm::floor(x / 16.f));
    int zFloor = static_cast<int>(glm::floor(z / 16.f));
    return m_chunks[toKey(16 * xFloor, 16 * zFloor)];
}

bool Terrain::hasTerrainAt(int x, int z) {
    if(m_generatedTerrain.count(toKey(x, z)) > 0) {
        return true;
    }
    else {
        return false;
    }
}


// uPtr<Chunk>& Terrain::getChunkAt(int x, int z) {
//     int xFloor = static_cast<int>(glm::floor(x / 16.f));
//     int zFloor = static_cast<int>(glm::floor(z / 16.f));
//     return m_chunks.at(toKey(16 * xFloor, 16 * zFloor));
// }

void Terrain::setGlobalBlockAt(int x, int y, int z, BlockType t)
{
    chunkMutex.lock();
    if(hasChunkAt(x, z)) {
        uPtr<Chunk> &c = getChunkAt(x, z);
        glm::vec2 chunkOrigin = glm::vec2(floor(x / 16.f) * 16, floor(z / 16.f) * 16);
        c->setLocalBlockAt(static_cast<unsigned int>(x - chunkOrigin.x),
                           static_cast<unsigned int>(y),
                           static_cast<unsigned int>(z - chunkOrigin.y),
                           t);
        // c->createVBOdata();
        chunkMutex.unlock();
    }
    else {
        chunkMutex.unlock();
        throw std::out_of_range("Coordinates " + std::to_string(x) +
                                " " + std::to_string(y) + " " +
                                std::to_string(z) + " have no Chunk!");
    }
}

void Terrain::loadChunkVBOs() {
    for (const auto& [key, value] : m_chunks) {
        if(value->ready && !value->loaded) {
            std::cout << "Creating VBO Data" << std::endl;
            value->ready = false;
            value->working = false;
            auto x = value.get();
            auto f = [x]() {
                std::cout << "Beginning VBO data Generation" << std::endl;
                x->generateVBOData();
            };

            auto VBOWorker = std::thread(f);
            VBOWorker.detach();
        }

        if(value->working) { ///DONE WORKING
            std::cout << "mew" << std::endl;
            value->loadToGPU();
            value->working = false;
            value->loaded = true;
        }
    }
}

Chunk* Terrain::instantiateChunkAt(int x, int z) {
    chunkMutex.lock();
    uPtr<Chunk> chunk = mkU<Chunk>(x, z, mp_context);
    Chunk *cPtr = chunk.get();
    m_chunks[toKey(x, z)] = std::move(chunk);
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
    chunkMutex.unlock();
    return cPtr;
}

// TODO: When you make Chunk inherit from Drawable, change this code so
// it draws each Chunk with the given ShaderProgram
void Terrain::draw(int minX, int maxX, int minZ, int maxZ, ShaderProgram *shaderProgram) {

        for(int x = minX; x < maxX; x += 16) {
            for(int z = minZ; z < maxZ; z += 16) {
                if (hasChunkAt(x, z)) {
                    const uPtr<Chunk> &chunk = getChunkAt(x, z);
                    if(chunk->loaded) {
                        shaderProgram->drawOpq(*chunk);
                    }
                }
            }
        }

        for(int x = minX; x < maxX; x += 16) {
            for(int z = minZ; z < maxZ; z += 16) {
                if (hasChunkAt(x, z)) {
                    const uPtr<Chunk> &chunk = getChunkAt(x, z);
                    if(chunk->loaded) {
                        shaderProgram->drawTrans(*chunk);
                    }
                }
            }
        }
}


void Terrain::CreateTestScene() {
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

    setGlobalBlockAt(0, 128, 0, WATER);
    setGlobalBlockAt(1, 128, 0, GRASS);


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
// void Terrain::expandTerrainIfNeeded(const glm::vec3 &playerPos) {
//     int xCenter = ((int)playerPos.x) - ((int)playerPos.x % 16);
//     int zCenter = ((int)playerPos.z) - ((int)playerPos.z % 16);

//     for(int i = -1; i <= 1; i++) {
//         for (int j = -1; j <= 1; j++) {
//             if(!hasChunkAt(xCenter + i * 16, zCenter + j * 16)) {
//                 // std::cout << "expanding Terrain" << std::endl;
//                 instantiateChunkAt(xCenter + i * 16, zCenter + j * 16);
//             }
//         }
//     }

//     xCenter = ((int)playerPos.x) - ((int)playerPos.x % 64);
//     zCenter = ((int)playerPos.z) - ((int)playerPos.z % 64);

//     for(int i = -1; i <= 1; i++) {
//         for (int j = -1; j <= 1; j++) {
//             int c = m_generatedTerrain.count(toKey(xCenter + 64*i, zCenter + 64 * j));
//             if (c == 0) {
//                 // std::cout << "expanding generation" << std::endl;
//                 m_generatedTerrain.insert(toKey(xCenter + 64*i, zCenter + 64 * j));
//             }
//         }
//     }
// }


float PerlinNoise(float x, float y, float z);


void Terrain::GenerateTerrain(int xPos, int zPos)  {

    if(m_generatedTerrain.count(toKey(xPos, zPos)) > 0) {
        return;
    }

    m_generatedTerrain.insert(toKey(xPos, zPos));

    int WinChunks = 4;

    for(int x = xPos; x < 16*WinChunks + xPos; x += 16) {
        for(int z = zPos; z < 16*WinChunks + zPos; z += 16) {
            if(hasChunkAt(x, z)) {
                auto *c = getChunkAt(x, z).get();
                c->ready = false;
            }
        }
    }

    std::cout << "Generating Terrain at " << xPos << ", " << zPos << std::endl;

    // Create the Chunks that will
    // store the blocks for our
    // initial world space
    for(int x = xPos; x < 16*WinChunks + xPos; x += 16) {
        for(int z = zPos; z < 16*WinChunks + zPos; z += 16) {
            instantiateChunkAt(x, z);
        }
    }

    std::cout << "Part 1 for at " << xPos << ", " << zPos << std::endl;

    // Create the basic terrain floor
    for(int y = 0; y < 256; y++) {
        for (int x = xPos; x < 16*WinChunks + xPos; x++) {
            for (int z = zPos; z < 16*WinChunks + zPos; z++) {
                float noise;
                bool isEmpty = true;

                float typeTerrain = PerlinNoise(x * 0.01f * 1.5346 + 0.3246, z * 0.01f * 1.5346 + 0.8756, 0);
                if (y == 0) {
                    setGlobalBlockAt(x, y, z, BEDROCK);
                    isEmpty = false;
                } else if (y <= 128) {
                    noise = PerlinNoise(0.1*x, 0.1*z, 0.1*y);
                    // I know instructions say negative but I find this produces a nice looking result
                    if (noise < 0.3) {
                        if (y<25) {
                            setGlobalBlockAt(x, y, z, LAVA);
                            isEmpty = false;
                        } else {
                            setGlobalBlockAt(x, y, z, EMPTY);
                        }
                    } else {
                        setGlobalBlockAt(x, y, z, STONE);
                        isEmpty = false;
                    }
                } else if (y == 129) {
                    noise = PerlinNoise(0.05 * x * 0.6237f, 0.05 * z * 0.6237f, 0);
                    if (typeTerrain < 0.5) {
                        if(noise >= (0.3)) {
                            setGlobalBlockAt(x, y, z, GRASS);
                            isEmpty = false;
                        }
                    } else {
                        if(noise >= (0.3)) {
                            setGlobalBlockAt(x, y, z, STONE);
                            isEmpty = false;
                        }
                    }
                }
                else {
                    noise = PerlinNoise(0.05 * x * 0.6237f, 0.05 * z * 0.6237f, 0);
                    float rand;
                    if (typeTerrain < 0.5) {
                        rand = noise - 0.06 * (y - 130);

                        if(rand > 0.35f) {
                            setGlobalBlockAt(x, y, z, GRASS);
                            isEmpty = false;
                        }
                    } else {
                        rand = (noise * glm::min(2.0f, 1.0f)) - 0.03 * (y - 130);
                        // rand *= typeTerrain;

                        if(rand > 0.35f) {
                            setGlobalBlockAt(x, y, z, STONE);
                            isEmpty = false;
                        }
                    }
                }

                if(isEmpty && y < 130 && y >= 125) {
                    setGlobalBlockAt(x, y, z, WATER);
                }
            }
        }
    }


    // std::cout << "Part 2 for at " << xPos << ", " << zPos << std::endl;



    // for(int y = 0; y < 255; y++) {
    //     for (int x = xPos; x < 16*WinChunks + xPos; x++) {
    //         for (int z = zPos; z < 16*WinChunks + zPos; z++) {
    //             if (y <= 8 * PerlinNoise(x / 20.37, 100, z /20.37) + 60) {
    //                 setGlobalBlockAt(x, y, z, STONE);
    //             }
    //         }
    //     }
    // }

    std::cout << "Part 3 for at " << xPos << ", " << zPos << std::endl;

    for(int x = xPos; x < 16*WinChunks + xPos; x += 16) {
        for(int z = zPos; z < 16*WinChunks + zPos; z += 16) {
            auto *c = getChunkAt(x, z).get();
            c->ready = true;
        }
    }


    std::cout << "Success at " << xPos << ", " << zPos << std::endl;
}



float PerlinNoise(float x, float y, float z) {
    // Fade function to smooth the interpolation
    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); };

    // Gradient function (returns a pseudo-random gradient value)
    auto grad = [](int hash, float x, float y, float z) -> float {
        int h = hash & 15;
        float u = (h < 8) ? x : y;
        float v = (h < 4) ? y : (h == 12 || h == 14) ? x : z;
        return (h & 1 ? -u : u) + (h & 2 ? -v : v) + (h & 4 ? -z : z);
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
    auto hash = [&](int x, int y, int z) -> int {
        return p[(x + p[(y + p[z & 255]) & 255]) & 255];
    };

    // Determine grid cell coordinates
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    // Relative coordinates in grid cell
    float xf = x - std::floor(x);
    float yf = y - std::floor(y);
    float zf = z - std::floor(z);

    // Fade curves for smooth interpolation
    float u = fade(xf);
    float v = fade(yf);
    float w = fade(zf);

    // Hash coordinates of the 4 corners of the unit square
    int aaa = hash(X, Y, Z);
    int aab = hash(X, Y, Z + 1);
    int aba = hash(X, Y + 1, Z);
    int abb = hash(X, Y + 1, Z + 1);
    int baa = hash(X + 1, Y, Z);
    int bab = hash(X + 1, Y, Z + 1);
    int bba = hash(X + 1, Y + 1, Z);
    int bbb = hash(X + 1, Y + 1, Z + 1);

    // Interpolate gradients using fade function
    float gradAAA = grad(aaa, xf, yf, zf);
    float gradAAB = grad(aab, xf, yf, zf - 1);
    float gradABA = grad(aba, xf, yf - 1, zf);
    float gradABB = grad(abb, xf, yf - 1, zf - 1);
    float gradBAA = grad(baa, xf - 1, yf, zf);
    float gradBAB = grad(bab, xf - 1, yf, zf - 1);
    float gradBBA = grad(bba, xf - 1, yf - 1, zf);
    float gradBBB = grad(bbb, xf - 1, yf - 1, zf - 1);

    // Interpolate the results
    float x1 = glm::mix(gradAAA, gradBAA, u);
    float x2 = glm::mix(gradABA, gradBBA, u);
    float y1 = glm::mix(x1, x2, v);
    x1 = glm::mix(gradAAB, gradBAB, u);
    x2 = glm::mix(gradABB, gradBBB, u);
    float y2 = glm::mix(x1, x2, v);

    return glm::mix(y1, y2, w) * 0.5f + 0.5f; // Normalize the result to [0, 1]
}



// terrain functions







