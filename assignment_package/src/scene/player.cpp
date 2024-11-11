#include "player.h"
#include <QString>

Player::Player(glm::vec3 pos, const Terrain &terrain)
    : Entity(pos), m_velocity(0,0,0), m_acceleration(0,0,0),
      m_camera(pos + glm::vec3(0, 1.5f, 0)), mcr_terrain(terrain),
      mcr_camera(m_camera), m_movementMode(MovementMode::WALKING),
      m_mouseSensitivity(0.1f)
{}

Player::~Player()
{}

void Player::tick(float dT, InputBundle &input) {
    processInputs(input);
    computePhysics(dT, mcr_terrain);
}

void Player::processInputs(InputBundle &inputs) {
    // TODO: Update the Player's velocity and acceleration based on the
    // state of the inputs.
    // first, update look direction
    this->rotateOnRightLocal(inputs.mouseY * m_mouseSensitivity);
    this->rotateOnUpGlobal(inputs.mouseX * m_mouseSensitivity);

    this->m_acceleration = glm::vec3(0,0,0);
    // update acceleration
    switch (this->m_movementMode) {
        case MovementMode::WALKING:
            break;
        case MovementMode::FLYING:
            if (inputs.wPressed) {
                this->m_acceleration += this->m_forward;
            }
            if (inputs.sPressed) {
                this->m_acceleration -= this->m_forward;
            }
            if (inputs.dPressed) {
                this->m_acceleration += this->m_right;
            }
            if (inputs.aPressed) {
                this->m_acceleration -= this->m_right;
            }
            if (inputs.qPressed) {
                this->m_acceleration -= this->m_up;
            }
            if (inputs.ePressed) {
                this->m_acceleration += this->m_up;
            }
            if (glm::length(this->m_acceleration) > 0) {
                this->m_acceleration = glm::normalize(this->m_acceleration);
                this->m_acceleration *= 20.f;
            }
    }
}

void Player::computePhysics(float dT, const Terrain &terrain) {
    // TODO: Update the Player's position based on its acceleration
    // and velocity, and also perform collision detection.
    switch (this->m_movementMode) {
        case MovementMode::WALKING:
            break;
        case MovementMode::FLYING:
            // first, apply drag
            this->m_velocity *= glm::pow(0.1f, dT);
            // then, apply acceleration
            this->m_velocity += this->m_acceleration * dT;
            // then, apply velocity
            if (glm::length(this->m_velocity) > 23.f) {
                this->m_velocity = glm::normalize(this->m_velocity);
                this->m_velocity *= 23.f;
            }
            this->moveAlongVector(this->m_velocity * dT);
            break;
    }
}

void Player::setCameraWidthHeight(unsigned int w, unsigned int h) {
    m_camera.setWidthHeight(w, h);
}

void Player::moveAlongVector(glm::vec3 dir) {
    Entity::moveAlongVector(dir);
    m_camera.moveAlongVector(dir);
}
void Player::moveForwardLocal(float amount) {
    Entity::moveForwardLocal(amount);
    m_camera.moveForwardLocal(amount);
}
void Player::moveRightLocal(float amount) {
    Entity::moveRightLocal(amount);
    m_camera.moveRightLocal(amount);
}
void Player::moveUpLocal(float amount) {
    Entity::moveUpLocal(amount);
    m_camera.moveUpLocal(amount);
}
void Player::moveForwardGlobal(float amount) {
    Entity::moveForwardGlobal(amount);
    m_camera.moveForwardGlobal(amount);
}
void Player::moveRightGlobal(float amount) {
    Entity::moveRightGlobal(amount);
    m_camera.moveRightGlobal(amount);
}
void Player::moveUpGlobal(float amount) {
    Entity::moveUpGlobal(amount);
    m_camera.moveUpGlobal(amount);
}
void Player::rotateOnForwardLocal(float degrees) {
    Entity::rotateOnForwardLocal(degrees);
    m_camera.rotateOnForwardLocal(degrees);
}
void Player::rotateOnRightLocal(float degrees) {
    Entity::rotateOnRightLocal(degrees);
    m_camera.rotateOnRightLocal(degrees);
}
void Player::rotateOnUpLocal(float degrees) {
    Entity::rotateOnUpLocal(degrees);
    m_camera.rotateOnUpLocal(degrees);
}
void Player::rotateOnForwardGlobal(float degrees) {
    Entity::rotateOnForwardGlobal(degrees);
    m_camera.rotateOnForwardGlobal(degrees);
}
void Player::rotateOnRightGlobal(float degrees) {
    Entity::rotateOnRightGlobal(degrees);
    m_camera.rotateOnRightGlobal(degrees);
}
void Player::rotateOnUpGlobal(float degrees) {
    Entity::rotateOnUpGlobal(degrees);
    m_camera.rotateOnUpGlobal(degrees);
}

QString Player::posAsQString() const {
    std::string str("( " + std::to_string(m_position.x) + ", " + std::to_string(m_position.y) + ", " + std::to_string(m_position.z) + ")");
    return QString::fromStdString(str);
}
QString Player::velAsQString() const {
    std::string str("( " + std::to_string(m_velocity.x) + ", " + std::to_string(m_velocity.y) + ", " + std::to_string(m_velocity.z) + ")");
    return QString::fromStdString(str);
}
QString Player::accAsQString() const {
    std::string str("( " + std::to_string(m_acceleration.x) + ", " + std::to_string(m_acceleration.y) + ", " + std::to_string(m_acceleration.z) + ")");
    return QString::fromStdString(str);
}
QString Player::lookAsQString() const {
    std::string str("( " + std::to_string(m_forward.x) + ", " + std::to_string(m_forward.y) + ", " + std::to_string(m_forward.z) + ")");
    return QString::fromStdString(str);
}
