#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include <GL/glew.h>

class Texture
{
public:
    unsigned int ID;
    int Width, Height, nrChannels;

    Texture(const char* path);
    Texture(int width, int height, unsigned char* data, int channels = 3);
    ~Texture();

    void bind();
};

#endif
