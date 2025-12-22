#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aLight;
layout (location = 4) in vec2 aTexOrigin;

out vec4 ourColor;
out vec2 TexCoord;
out vec3 Lighting;
out vec2 TexOrigin;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    FragPos = vec3(worldPos);
    ourColor = aColor;
    TexCoord = aTexCoord;
    Lighting = aLight; // x=Sky, y=Block, z=AO
    TexOrigin = aTexOrigin;
}
