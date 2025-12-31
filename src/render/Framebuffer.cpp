#include "Framebuffer.h"
#include <iostream>

Framebuffer::Framebuffer(int width, int height)
    : m_FBO(0), m_TextureID(0), m_RBO(0), m_Width(width), m_Height(height) {
  Init();
}

Framebuffer::~Framebuffer() {
  glDeleteFramebuffers(1, &m_FBO);
  glDeleteTextures(1, &m_TextureID);
  glDeleteRenderbuffers(1, &m_RBO);
}

void Framebuffer::Init() {
  if (m_FBO) {
    glDeleteFramebuffers(1, &m_FBO);
    glDeleteTextures(1, &m_TextureID);
    glDeleteRenderbuffers(1, &m_RBO);
  }

  glGenFramebuffers(1, &m_FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

  // Color Texture Attachment
  glGenTextures(1, &m_TextureID);
  glBindTexture(GL_TEXTURE_2D, m_TextureID);

  // Use GL_RGBA to handle potential alpha properly or just RGB
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_Width, m_Height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         m_TextureID, 0);

  // Renderbuffer for Depth and Stencil
  glGenRenderbuffers(1, &m_RBO);
  glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width,
                        m_Height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, m_RBO);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!"
              << std::endl;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Bind() {
  glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
  glViewport(0, 0, m_Width, m_Height);
}

void Framebuffer::Unbind() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // Viewport should be reset by application or ImGui
}

void Framebuffer::Resize(int width, int height) {
  if (width != m_Width || height != m_Height) {
    m_Width = width;
    m_Height = height;
    Init();
  }
}
