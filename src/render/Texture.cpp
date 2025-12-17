#include "Texture.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../vendor/stb_image.h"

Texture::Texture(int width, int height, unsigned char* data, int channels)
    : Width(width), Height(height), nrChannels(channels)
{
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (data)
    {
        GLenum format = GL_RGB;
        if (channels == 4) format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, Width, Height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

Texture::Texture(const char* path)
{
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID); // all upcoming GL_TEXTURE_2D operations now have effect on this texture object
    
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Pixel art style
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // load image, create texture and generate mipmaps
    stbi_set_flip_vertically_on_load(true); 
    unsigned char *data = stbi_load(path, &Width, &Height, &nrChannels, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrChannels == 4)
            format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format, Width, Height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture: " << path << std::endl;
        // Create a fallback pink texture (error)
        unsigned char pink[] = {255, 0, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pink);
    }
    stbi_image_free(data);
}

Texture::~Texture()
{
    // glDeleteTextures(1, &ID); // Often we want to manage lifetime explicitly, but for now simple RAII is tricky if copied. 
    // ideally we disable copy or use ref counting. For this simple engine, we leaks or manuals.
    // Let's rely on manual cleanup or program termination. 
    // Actually, let's look at how it's used. If it's a member of a class...
}

void Texture::bind()
{
    glBindTexture(GL_TEXTURE_2D, ID);
}
