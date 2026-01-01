#pragma once

#include <GL/glew.h>
#include <string>
#include <vector>

// Noise preview texture generator and renderer
class NoisePreview {
public:
  enum class ColorScheme {
    Grayscale,   // 0=black, 1=white
    Terrain,     // Blue→Green→Yellow→Red→White
    Temperature, // Blue (cold) → Yellow (hot)
    EdgeDistance // Black (edge) → White (center)
  };

  NoisePreview(int width = 256, int height = 256);
  ~NoisePreview();

  // Generate texture from noise data
  void UpdateFromData(const float *data,
                      ColorScheme scheme = ColorScheme::Grayscale);

  // Get OpenGL texture ID for ImGui::Image()
  GLuint GetTextureID() const { return m_TextureID; }

  int GetWidth() const { return m_Width; }
  int GetHeight() const { return m_Height; }

private:
  void ApplyColorScheme(const float *input, unsigned char *output,
                        ColorScheme scheme);
  void NormalizeData(const float *input, float *output, int count);

  int m_Width;
  int m_Height;
  GLuint m_TextureID;
  std::vector<unsigned char> m_PixelBuffer; // RGBA buffer
};
