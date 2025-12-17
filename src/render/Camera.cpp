#include "Camera.h"
#include <iostream>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) 
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
{
    Position = position;
    WorldUp = up;
    Yaw = yaw;
    Pitch = pitch;
    // We don't verify vectors here as Player drives them, but nice to init.
    // Default Front
    Front = glm::vec3(0.0f, 0.0f, -1.0f);
    Right = glm::vec3(1.0f, 0.0f, 0.0f);
    Up = up;
}

glm::mat4 Camera::GetViewMatrix()
{
    return glm::lookAt(Position, Position + Front, Up);
}
