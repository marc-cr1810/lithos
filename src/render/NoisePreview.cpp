#include "NoisePreview.h"
#include <algorithm>
#include <cmath>

NoisePreview::NoisePreview(int width, int height)
    : m_Width(width), m_Height(height), m_TextureID(0) {
  m_PixelBuffer.resize(width * height * 4); // RGBA

  // Generate OpenGL texture
  glGenTextures(1, &m_TextureID);
  glBindTexture(GL_TEXTURE_2D, m_TextureID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

NoisePreview::~NoisePreview() {
  if (m_TextureID) {
    glDeleteTextures(1, &m_TextureID);
  }
}

void NoisePreview::UpdateFromData(const float *data, ColorScheme scheme) {
  // Normalize data first
  std::vector<float> normalized(m_Width * m_Height);
  NormalizeData(data, normalized.data(), m_Width * m_Height);

  // Apply color scheme
  ApplyColorScheme(normalized.data(), m_PixelBuffer.data(), scheme);

  // Upload to GPU
  glBindTexture(GL_TEXTURE_2D, m_TextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Width, m_Height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, m_PixelBuffer.data());
}

void NoisePreview::NormalizeData(const float *input, float *output, int count) {
  // Find min/max
  float minVal = input[0];
  float maxVal = input[0];
  for (int i = 1; i < count; ++i) {
    if (input[i] < minVal)
      minVal = input[i];
    if (input[i] > maxVal)
      maxVal = input[i];
  }

  // Normalize to [0, 1]
  float range = maxVal - minVal;
  if (range < 0.0001f)
    range = 1.0f; // Avoid division by zero

  for (int i = 0; i < count; ++i) {
    output[i] = (input[i] - minVal) / range;
  }
}

void NoisePreview::ApplyColorScheme(const float *input, unsigned char *output,
                                    ColorScheme scheme) {
  int count = m_Width * m_Height;

  for (int i = 0; i < count; ++i) {
    float val = std::clamp(input[i], 0.0f, 1.0f);
    int idx = i * 4;

    switch (scheme) {
    case ColorScheme::Grayscale: {
      unsigned char gray = static_cast<unsigned char>(val * 255);
      output[idx + 0] = gray; // R
      output[idx + 1] = gray; // G
      output[idx + 2] = gray; // B
      output[idx + 3] = 255;  // A
      break;
    }

    case ColorScheme::Terrain: {
      // Blue→Green→Yellow→Red→White
      unsigned char r, g, b;
      if (val < 0.25f) {
        // Blue to Cyan
        float t = val / 0.25f;
        r = 0;
        g = static_cast<unsigned char>(t * 255);
        b = 255;
      } else if (val < 0.5f) {
        // Cyan to Green
        float t = (val - 0.25f) / 0.25f;
        r = 0;
        g = 255;
        b = static_cast<unsigned char>((1.0f - t) * 255);
      } else if (val < 0.75f) {
        // Green to Yellow
        float t = (val - 0.5f) / 0.25f;
        r = static_cast<unsigned char>(t * 255);
        g = 255;
        b = 0;
      } else {
        // Yellow to Red to White
        float t = (val - 0.75f) / 0.25f;
        r = 255;
        g = static_cast<unsigned char>((1.0f - t * 0.5f) * 255);
        b = static_cast<unsigned char>(t * 255);
      }
      output[idx + 0] = r;
      output[idx + 1] = g;
      output[idx + 2] = b;
      output[idx + 3] = 255;
      break;
    }

    case ColorScheme::Temperature: {
      // Blue (cold) → Yellow (hot)
      unsigned char r, g, b;
      if (val < 0.5f) {
        // Blue to Cyan
        float t = val / 0.5f;
        r = 0;
        g = static_cast<unsigned char>(t * 200);
        b = 255;
      } else {
        // Cyan to Yellow
        float t = (val - 0.5f) / 0.5f;
        r = static_cast<unsigned char>(t * 255);
        g = 200 + static_cast<unsigned char>(t * 55);
        b = static_cast<unsigned char>((1.0f - t) * 255);
      }
      output[idx + 0] = r;
      output[idx + 1] = g;
      output[idx + 2] = b;
      output[idx + 3] = 255;
      break;
    }

    case ColorScheme::EdgeDistance: {
      // Black (edge) → White (center)
      unsigned char intensity = static_cast<unsigned char>(val * 255);
      output[idx + 0] = intensity;
      output[idx + 1] = intensity;
      output[idx + 2] = intensity;
      output[idx + 3] = 255;
      break;
    }
    }
  }
}
