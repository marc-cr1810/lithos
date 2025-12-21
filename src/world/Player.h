#ifndef PLAYER_H
#define PLAYER_H

#include <glm/glm.hpp>
#include <GL/glew.h>

class World;

enum Player_Movement {
    P_FORWARD,
    P_BACKWARD,
    P_LEFT,
    P_RIGHT,
    P_UP,
    P_DOWN
};

class Player
{
public:
    // Attributes
    glm::vec3 Position;
    glm::vec3 Velocity;
    glm::vec3 Front;
    glm::vec3 Right;
    glm::vec3 Up;
    glm::vec3 WorldUp;
    
    // Euler Angles
    float Yaw;
    float Pitch;
    
    // Options
    float MovementSpeed;
    float MouseSensitivity;
    
    // Physics
    float Gravity;
    float JumpForce;
    bool IsGrounded;
    bool FlyMode = false;
    
    // Checks if the player at the given position collides with any blocks
    bool CheckCollision(glm::vec3 pos, const World& world);

    Player(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

    void ProcessKeyboard(Player_Movement direction, float deltaTime, const World& world);
    void ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch = true);
    void ProcessJump();
    
    void Update(float deltaTime, const World& world);
    
    glm::vec3 GetEyePosition() const;

private:
    void updateCameraVectors();
};

#endif
