#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <GL/glew.h>

class Framebuffer {
public:
  Framebuffer(int width, int height);
  ~Framebuffer();

  void Bind();
  void Unbind();
  void Resize(int width, int height);
  unsigned int GetTextureID() const { return m_TextureID; }

  int m_Width;
  int m_Height;

private:
  unsigned int m_FBO;
  unsigned int m_TextureID;
  unsigned int m_RBO;

  void Init();
};

#endif
