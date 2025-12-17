#include "Player.h"
#include "World.h"
#include <cmath>
#include <algorithm>
#include <iostream>

Player::Player(glm::vec3 position) 
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(5.0f), MouseSensitivity(0.1f), 
      Yaw(-90.0f), Pitch(0.0f), WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      Velocity(0.0f), Gravity(18.0f), JumpForce(8.0f), IsGrounded(false)
{
    Position = position;
    updateCameraVectors();
}

// Calculates the front vector from the Camera's (Euler) Angles
void Player::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp)); 
    Up    = glm::normalize(glm::cross(Right, Front));
}

void Player::ProcessKeyboard(Player_Movement direction, float deltaTime, const World& world)
{
    float velocity = MovementSpeed * deltaTime;
    glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
    glm::vec3 flatRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

    glm::vec3 moveDir(0.0f);
    if (direction == P_FORWARD) moveDir += flatFront;
    if (direction == P_BACKWARD) moveDir -= flatFront;
    if (direction == P_LEFT) moveDir -= flatRight;
    if (direction == P_RIGHT) moveDir += flatRight;
    
    if (glm::length(moveDir) > 0.0f)
    {
         moveDir = glm::normalize(moveDir) * velocity;
         
         // Try X
         glm::vec3 tryX = Position;
         tryX.x += moveDir.x;
         if (!CheckCollision(tryX, world))
             Position.x = tryX.x;
             
         // Try Z
         glm::vec3 tryZ = Position;
         tryZ.z += moveDir.z;
         // Use current X
         tryZ.x = Position.x;
         if (!CheckCollision(tryZ, world))
             Position.z = tryZ.z;
    }
}

void Player::ProcessMouseMovement(float xoffset, float yoffset, GLboolean constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // Update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Player::ProcessJump()
{
    if (IsGrounded)
    {
        Velocity.y = JumpForce;
        IsGrounded = false;
    }
}

void Player::Update(float deltaTime, const World& world)
{
    // Apply Gravity
    Velocity.y -= Gravity * deltaTime;
    // Terminal Velocity
    if (Velocity.y < -50.0f) Velocity.y = -50.0f;

    // Check Ceiling Collision
    if (Velocity.y > 0.0f) {
        glm::vec3 tryPos = Position;
        tryPos.y += Velocity.y * deltaTime;
        if (CheckCollision(tryPos, world)) {
             Velocity.y = 0.0f;
             // Don't move up
        }
    }

    // Apply Movement
    Position.y += Velocity.y * deltaTime;

    // Floor Collision
    float playerHeight = 1.8f;
    float eyeHeight = 1.6f;
    float feetY = Position.y - eyeHeight;
    int blockY = (int)floor(feetY);

    float playerWidth = 0.6f;
    
    int minBlockX = (int)floor(Position.x - playerWidth / 2.0f);
    int maxBlockX = (int)floor(Position.x + playerWidth / 2.0f);
    int minBlockZ = (int)floor(Position.z - playerWidth / 2.0f);
    int maxBlockZ = (int)floor(Position.z + playerWidth / 2.0f);
    
    bool hitGround = false;
    
    // Only check floor collision if we are falling or standing still
    // This prevents snapping to the top of blocks we are jumping through/past
    if (Velocity.y <= 0.0f && blockY >= -128 && blockY < 512) 
    {
        for(int x = minBlockX; x <= maxBlockX; ++x)
        {
            for(int z = minBlockZ; z <= maxBlockZ; ++z)
            {
                if (world.getBlock(x, blockY, z).isActive())
                {
                    hitGround = true;
                    x = maxBlockX + 1; 
                    break;
                }
            }
        }
    }
    
    if (hitGround)
    {
         Position.y = (float)(blockY + 1) + eyeHeight;
         Velocity.y = 0.0f;
         IsGrounded = true;
    }
    else
    {
         IsGrounded = false;
    }
}

bool Player::CheckCollision(glm::vec3 pos, const World& world)
{
    float playerWidth = 0.6f;
    float playerHeight = 1.8f; 
    float eyeHeight = 1.6f;
    
    // Shrink the bounding box slightly to prevent treating "touching" as overlapping
    float epsilon = 0.05f; // Small buffer
    float minX = pos.x - playerWidth / 2.0f;
    float maxX = pos.x + playerWidth / 2.0f;
    float minZ = pos.z - playerWidth / 2.0f;
    float maxZ = pos.z + playerWidth / 2.0f;
    // Contract Y to allow sliding on floor/ceiling without getting stuck
    float minY = pos.y - eyeHeight + epsilon;
    float maxY = pos.y - eyeHeight + playerHeight - epsilon;

    int minBlockX = (int)floor(minX);
    int maxBlockX = (int)floor(maxX);
    int minBlockY = (int)floor(minY);
    int maxBlockY = (int)floor(maxY);
    int minBlockZ = (int)floor(minZ);
    int maxBlockZ = (int)floor(maxZ);

    for(int x = minBlockX; x <= maxBlockX; ++x)
    {
        for(int y = minBlockY; y <= maxBlockY; ++y)
        {
            for(int z = minBlockZ; z <= maxBlockZ; ++z)
            {
                if(world.getBlock(x, y, z).isActive())
                    return true;
            }
        }
    }
    return false;
}

glm::vec3 Player::GetEyePosition() const
{
    return Position;
}
