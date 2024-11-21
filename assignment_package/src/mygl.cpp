#include "mygl.h"
#include <glm_includes.h>

#include <iostream>
#include <QApplication>
#include <QKeyEvent>
#include <QDateTime>
#include <thread>
#include <QFile>


MyGL::MyGL(QWidget *parent)
    : OpenGLContext(parent),
      m_worldAxes(this),
      m_progLambert(this), m_progFlat(this), m_progInstanced(this), m_texture(this),
      m_terrain(this), m_player(glm::vec3(48.f, 129.f, 48.f), m_terrain),
    m_inputs(), m_timer(), m_time(0.f), m_lastTime(QDateTime::currentMSecsSinceEpoch())
{
    // Connect the timer to a function so that when the timer ticks the function is executed
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
    // Tell the timer to redraw 60 times per second
    m_timer.start(16);
    setFocusPolicy(Qt::ClickFocus);

    setMouseTracking(true); // MyGL will track the mouse's movements even if a mouse button is not pressed
    setCursor(Qt::BlankCursor); // Make the cursor invisible
}

MyGL::~MyGL() {
    makeCurrent();
    glDeleteVertexArrays(1, &vao);
}


void MyGL::moveMouseToCenter() {
    QCursor::setPos(this->mapToGlobal(QPoint(width() / 2, height() / 2)));
}

void MyGL::initializeGL()
{
    // Create an OpenGL context using Qt's QOpenGLFunctions_3_2_Core class
    // If you were programming in a non-Qt context you might use GLEW (GL Extension Wrangler)instead
    initializeOpenGLFunctions();
    // Print out some information about the current OpenGL context
    debugContextVersion();

    // Set a few settings/modes in OpenGL rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // Set the color with which the screen is filled at the start of each render call.
    glClearColor(0.37f, 0.74f, 1.0f, 1);

    printGLErrorLog();

    // Create a Vertex Attribute Object
    glGenVertexArrays(1, &vao);

    //Create the instance of the world axes
    m_worldAxes.createVBOdata();

    // Create and set up the diffuse shader
    m_progLambert.create(":/glsl/lambert.vert.glsl", ":/glsl/lambert.frag.glsl");
    // Create and set up the flat lighting shader
    m_progFlat.create(":/glsl/flat.vert.glsl", ":/glsl/flat.frag.glsl");
    // m_progInstanced.create(":/glsl/instanced.vert.glsl", ":/glsl/lambert.frag.glsl");


if (!QFile(":/textures/minecraft_textures_all.png").exists()){
        std::cerr << "error: tex file not found" << std::endl;
    } else {
        m_texture.create(":/textures/minecraft_textures_all.png", GL_RGBA, GL_RGBA);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //transparency effect


    int x = (m_player.mcr_position.x - 32) - ((int)(m_player.mcr_position.x - 32) % 64);
    int z = (m_player.mcr_position.z - 32) - ((int)(m_player.mcr_position.z - 32) % 64);

    for(int i = -1; i <= 1; i++) {
        for(int j = -1; j <= 1; j++) {
            // std::cout << x + i * 64 << ", " << z + j * 64 << std::endl;
            m_terrain.GenerateTerrain(x + i * 64, z + j * 64);
            // std::cout << "COokie" << std::endl;
        }
    }

    // We have to have a VAO bound in OpenGL 3.2 Core. But if we're not
    // using multiple VAOs, we can just bind one once.
    glBindVertexArray(vao);
}


void MyGL::resizeGL(int w, int h) {
    //This code sets the concatenated view and perspective projection matrices used for
    //our scene's camera view.
    m_player.setCameraWidthHeight(static_cast<unsigned int>(w), static_cast<unsigned int>(h));
    glm::mat4 viewproj = m_player.mcr_camera.getViewProj();

    // Upload the view-projection matrix to our shaders (i.e. onto the graphics card)

    m_progLambert.setUnifMat4("u_ViewProj", viewproj);
    m_progFlat.setUnifMat4("u_ViewProj", viewproj);
    m_progInstanced.setUnifMat4("u_ViewProj", viewproj);

    printGLErrorLog();
}


// MyGL's constructor links tick() to a timer that fires 60 times per second.
// We're treating MyGL as our game engine class, so we're going to perform
// all per-frame actions here, such as performing physics updates on all
// entities in the scene.
void MyGL::tick() {
    // Calculate dT
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    float dT = (currentTime - m_lastTime) / 1000.f;
    if (dT > 0) {
        m_lastTime = currentTime;
    }
    m_player.tick(dT, m_inputs); // Player-side tick

    int x = (m_player.mcr_position.x - 32) - ((int)(m_player.mcr_position.x - 32) % 64);
    int z = (m_player.mcr_position.z - 32) - ((int)(m_player.mcr_position.z - 32) % 64);


    auto f = [this](float x, float z) {
        m_terrain.GenerateTerrain(x, z);
    };

    std::vector<std::thread> blockTypeWorkers = {};

    for(int i = -2; i <= 2; i++) {
        for(int j = -2; j <= 2; j++) {
            // std::cout << x + i * 64 << ", " << z + j * 64 << std::endl;
            if(!m_terrain.hasTerrainAt(x + i * 64, z + j * 64)) {
                 std::cout << "gen thread for terrain " << x + i * 64 << ", " << z + j * 64 << std::endl;
                 blockTypeWorkers.push_back(std::thread(f, x + i * 64, z + j * 64));
                // f(x + i * 64, z + j * 64);
            }
            // std::cout << "COokie" << std::endl;
        }
    }

    m_terrain.loadChunkVBOs();

    for(auto &x : blockTypeWorkers) {
        x.detach();
    }

    m_inputs.mouseX = 0;
    m_inputs.mouseY = 0;
    m_progLambert.setUnifFloat("u_Time", m_time);
    update(); // Calls paintGL() as part of a larger QOpenGLWidget pipeline
    //check terrain expansion
    // m_terrain.expandTerrainIfNeeded(m_player.mcr_position);
    sendPlayerDataToGUI(); // Updates the info in the secondary window displaying player data
}

void MyGL::sendPlayerDataToGUI() const {
    emit sig_sendPlayerPos(m_player.posAsQString());
    emit sig_sendPlayerVel(m_player.velAsQString());
    emit sig_sendPlayerAcc(m_player.accAsQString());
    emit sig_sendPlayerLook(m_player.lookAsQString());
    glm::vec2 pPos(m_player.mcr_position.x, m_player.mcr_position.z);
    glm::ivec2 chunk(16 * glm::ivec2(glm::floor(pPos / 16.f)));
    glm::ivec2 zone(64 * glm::ivec2(glm::floor(pPos / 64.f)));
    emit sig_sendPlayerChunk(QString::fromStdString("( " + std::to_string(chunk.x) + ", " + std::to_string(chunk.y) + " )"));
    emit sig_sendPlayerTerrainZone(QString::fromStdString("( " + std::to_string(zone.x) + ", " + std::to_string(zone.y) + " )"));
}

// This function is called whenever update() is called.
// MyGL's constructor links update() to a timer that fires 60 times per second,
// so paintGL() called at a rate of 60 frames per second.
void MyGL::paintGL() {
    // Clear the screen so that we only see newly drawn images
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);


    glm::mat4 viewproj = m_player.mcr_camera.getViewProj();
    m_progLambert.setUnifMat4("u_ViewProj", viewproj);
    m_progLambert.setUnifMat4("u_Model", glm::mat4());
    m_progLambert.setUnifMat4("u_ModelInvTr", glm::mat4());
    m_progFlat.setUnifMat4("u_ViewProj", viewproj);
    m_progInstanced.setUnifMat4("u_ViewProj", viewproj);

    m_progLambert.setUnifFloat("u_Time", m_time++);

    m_texture.bind(0);
    renderTerrain();

    glDisable(GL_DEPTH_TEST);
    m_progFlat.setUnifMat4("u_Model", glm::mat4());
    m_progFlat.drawOpq(m_worldAxes);
    glEnable(GL_DEPTH_TEST);
}

// TODO: Change this so it renders the nine zones of generated
// terrain that surround the player (refer to Terrain::m_generatedTerrain
// for more info)
void MyGL::renderTerrain() {
    int x = m_player.mcr_position.x;
    int z = m_player.mcr_position.z;

    // for(int i = -1; i <= 1; i++) {
    //     for(int j = -1; j <= 1; j++) {
    //         int xCoord = x - 32 + i * 64;
    //         int zCoord = z - 32 + j * 64;

    //         // m_terrain.GenerateTerrain(xCoord, zCoord);
    //     }
    // }

    m_terrain.draw( x - 64, x + 64, z-64, z+64, &m_progLambert);

}


void MyGL::keyPressEvent(QKeyEvent *e) {
    float amount = 2.0f;
    if(e->modifiers() & Qt::ShiftModifier){
        amount = 10.0f;
    }
    switch (e->key()) {
        case Qt::Key_Escape:
            QApplication::quit();
            break;
        case Qt::Key_Right:
            m_player.rotateOnUpGlobal(-amount);
            break;
        case Qt::Key_Left:
            m_player.rotateOnUpGlobal(amount);
            break;
        case Qt::Key_Up:
            m_player.rotateOnRightLocal(-amount);
            break;
        case Qt::Key_Down:
            m_player.rotateOnRightLocal(amount);
            break;
        case Qt::Key_W:
            m_inputs.wPressed = true;
            break;
        case Qt::Key_S:
            m_inputs.sPressed = true;
            break;
        case Qt::Key_D:
            m_inputs.dPressed = true;
            break;
        case Qt::Key_A:
            m_inputs.aPressed = true;
            break;
        case Qt::Key_Q:
            m_inputs.qPressed = true;
            break;
        case Qt::Key_E:
            m_inputs.ePressed = true;
            break;
        case Qt::Key_Space:
            m_inputs.spacePressed = true;
            break;
        case Qt::Key_F:
            switch (m_player.m_movementMode) {
                case MovementMode::WALKING:
                    m_player.m_movementMode = MovementMode::FLYING;
                    break;
                case MovementMode::FLYING:
                    m_player.m_movementMode = MovementMode::WALKING;
                    break;
            }
        default:
            break;
    }
}

void MyGL::keyReleaseEvent(QKeyEvent *e) {
    switch (e->key()) {
        case Qt::Key_W:
            m_inputs.wPressed = false;
            break;
        case Qt::Key_S:
            m_inputs.sPressed = false;
            break;
        case Qt::Key_D:
            m_inputs.dPressed = false;
            break;
        case Qt::Key_A:
            m_inputs.aPressed = false;
            break;
        case Qt::Key_Q:
            m_inputs.qPressed = false;
            break;
        case Qt::Key_E:
            m_inputs.ePressed = false;
            break;
        case Qt::Key_Space:
            m_inputs.spacePressed = false;
            break;
        default:
            break;
    }
}

void MyGL::mouseMoveEvent(QMouseEvent *e) {
    // update m_inputs.mouseX and m_inputs.mouseY
    // based on the change in the mouse's position
    // since the last change.
    m_inputs.mouseX += width() / 2 - e->position().x();
    m_inputs.mouseY += height() / 2 - e->position().y();
    moveMouseToCenter();
}

void MyGL::mousePressEvent(QMouseEvent *e) {
    // generate ray
    glm::vec3 ray = m_player.mcr_camera.getLook();
    glm::vec3 currPos = m_player.mcr_camera.mcr_position;
    while (glm::length(currPos - m_player.mcr_camera.mcr_position) <= 3) {
        float nextX = ray.x >= 0 ? std::ceil(currPos.x + 0.01f) : std::floor(currPos.x-0.01f);
        float nextY = ray.y >= 0 ? std::ceil(currPos.y + 0.01f) : std::floor(currPos.y-0.01f);
        float nextZ = ray.z >= 0 ? std::ceil(currPos.z + 0.01f) : std::floor(currPos.z-0.01f);
        float xDist = (nextX - currPos.x) / ray.x;
        float yDist = (nextY - currPos.y) / ray.y;
        float zDist = (nextZ - currPos.z) / ray.z;
        float minDist = std::min(xDist, std::min(yDist, zDist));
        currPos += minDist * ray;
        if (xDist == minDist) {
            currPos.x = glm::round(currPos.x);
            currPos.x = ray.x >= 0 ? currPos.x + 0.01f : currPos.x - 0.01f;
        } else if (yDist == minDist) {
            currPos.y = glm::round(currPos.y);
            currPos.y = ray.y >= 0 ? currPos.y + 0.01f : currPos.y - 0.01f;
        } else {
            currPos.z = glm::round(currPos.z);
            currPos.z = ray.z >= 0 ? currPos.z + 0.01f : currPos.z - 0.01f;
        }
        if (m_terrain.hasChunkAt(currPos.x, currPos.z) && m_terrain.getGlobalBlockAt(currPos.x, currPos.y, currPos.z) != EMPTY) {
            switch (e->button()) {
                case Qt::LeftButton:
                std::cout << "remove block" << std::endl;
                    m_terrain.setGlobalBlockAt(currPos.x, currPos.y, currPos.z, EMPTY);
                    m_terrain.getChunkAt(currPos.x, currPos.z)->createVBOdata();
                    //ERROR: WHEN DESTROYING THE BLOCK AT THE EDGE OF A CHUNK, WE DO NOT UPDATE THE VBO OF THE NEIGHBORING CHUNK
                    break;
                case Qt::RightButton:
                {
                    glm::vec3 shift = glm::vec3(0.f);
                    if (xDist == minDist) {
                        shift.x += ray.x >= 0 ? -1 : 1;
                    }
                    if (yDist == minDist) {
                        shift.y += ray.y >= 0 ? -1 : 1;
                    }
                    if (zDist == minDist) {
                        shift.z += ray.z >= 0 ? -1 : 1;
                    }
                    if (m_terrain.hasChunkAt(currPos.x + shift.x, currPos.z + shift.z) && m_terrain.getGlobalBlockAt(currPos.x + shift.x, currPos.y + shift.y, currPos.z + shift.z) == EMPTY) {
                        m_terrain.setGlobalBlockAt(currPos.x + shift.x, currPos.y + shift.y, currPos.z + shift.z, GRASS);
                        m_terrain.getChunkAt(currPos.x + shift.x, currPos.z + shift.z)->createVBOdata();
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
    }
}
