#include "TextureAtlas.h"
#include <algorithm>
#include <cmath>
#include <cstring> // for memcpy
#include <filesystem>
#include <fstream>
#include <iostream>

#include <GL/glew.h>

// STB Image (Already implemented in Texture.cpp, but we need definitions if we
// want to use functions?) Actually stb_image implementation logic is strictly
// in one file. We just need the header.
#include "../vendor/stb_image.h"

namespace fs = std::filesystem;

TextureAtlas::TextureAtlas(int width, int height, int slotSize)
    : width(width), height(height), slotSize(slotSize), nextSlotX(0),
      nextSlotY(0), dirty(false) {
  // Initialize transparent black
  data.resize(width * height * 4, 0);
}

TextureAtlas::~TextureAtlas() {}

void TextureAtlas::Load(const std::string &directory) {
  if (!fs::exists(directory)) {
    std::cerr << "TextureAtlas Error: Directory not found " << directory
              << std::endl;
    return;
  }

  std::cout << "Loading textures from " << directory << "..." << std::endl;

  for (const auto &entry : fs::directory_iterator(directory)) {
    if (entry.path().extension() == ".png") {
      std::string path = entry.path().string();
      std::string filename = entry.path().filename().string();
      // Remove extension for name
      std::string name = entry.path().stem().string();

      // Load Image
      int w, h, c;
      // Force 4 channels (RGBA)
      stbi_set_flip_vertically_on_load(true);
      unsigned char *img = stbi_load(path.c_str(), &w, &h, &c, 4);
      if (img) {
        // Check for metadata
        int frameTime = 1;
        bool animated = false;

        std::string jsonPath = path + ".json";
        if (fs::exists(jsonPath)) {
          // Simple parse for "frametime"
          std::ifstream f(jsonPath);
          std::string content((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

          size_t pos = content.find("\"frametime\"");
          if (pos != std::string::npos) {
            // Find colon
            size_t colon = content.find(':', pos);
            if (colon != std::string::npos) {
              // Parse number
              frameTime = std::stoi(content.substr(colon + 1));
              animated = true;
            }
          } else {
            // Check if "animation" object exists, maybe default frametime?
            if (content.find("\"animation\"") != std::string::npos) {
              animated = true;
              frameTime = 1; // Default if not detailed?
            }
          }
        }

        // If height > width (strip), and not confirmed animated by JSON?
        // In MC dealing, usually strip implies animation if N*width = height.
        if (h > w && h % w == 0 && (h / w) > 1) {
          // Auto-detect animation if not specified, or if we want to support it
          // without JSON But let's stick to explicit or implicit logic. Let's
          // assume strip = animation for block textures if explicit JSON wasn't
          // found but format looks like one? User said "Make note of .json
          // files... ensure support". Let's rely on aspect ratio too.
          if (!animated) {
            animated = true;
            frameTime = 20; // Default slow?
          }
        }

        // Resize if larger than slotSize (Nearest Neighbor)
        if (w > slotSize && w % slotSize == 0) {
          int scale = w / slotSize;
          int newW = slotSize;
          int newH = h / scale;
          std::vector<unsigned char> resizedData(newW * newH * 4);

          for (int y = 0; y < newH; ++y) {
            for (int x = 0; x < newW; ++x) {
              int srcX = x * scale;
              int srcY = y * scale;
              int srcIdx = (srcY * w + srcX) * 4;
              int destIdx = (y * newW + x) * 4;

              resizedData[destIdx + 0] = img[srcIdx + 0]; // R
              resizedData[destIdx + 1] = img[srcIdx + 1]; // G
              resizedData[destIdx + 2] = img[srcIdx + 2]; // B
              resizedData[destIdx + 3] = img[srcIdx + 3]; // A
            }
          }

          // Replace img with resized data
          stbi_image_free(img);
          img = (unsigned char *)malloc(newW * newH * 4);
          memcpy(img, resizedData.data(), newW * newH * 4);
          w = newW;
          h = newH;
        }

        int frames = 1;
        if (animated && w > 0)
          frames = h / w;

        PackTexture(name, img, w, h, 4, frames, frameTime);

        stbi_image_free(img);
      } else {
        std::cerr << "Failed to load texture: " << filename
                  << " Reason: " << stbi_failure_reason() << std::endl;
      }
    }
  }

  std::cout << "Texture Atlas Loaded. " << textures.size()
            << " textures packed." << std::endl;
}

void TextureAtlas::PackTexture(const std::string &name, unsigned char *imgData,
                               int w, int h, int channels, int frameCount,
                               int frameTime) {
  // Only support square slots for now (or frame width = slotSize)
  // If texture is larger/smaller, we resize or just attempt to fit in slot?
  // For this task, assuming pixel art 16x16 blocks.
  int frameW = w;
  int frameH = h / frameCount;

  if (frameW != slotSize || frameH != slotSize) {
    // Warn? Or just accept?
    // If it's 16x16 vs 32x32, mixing resolutions in one atlas is messy if we
    // use grid. But we initialized atlas with slotSize=16. If we load a 32x32
    // texture, it won't fit in 16x16 grid easily without flexible packing. But
    // let's assume assets are compliant or we just write what we can.
  }

  // Find Slot
  if (nextSlotX * slotSize >= width) {
    nextSlotX = 0;
    nextSlotY++;
  }
  if (nextSlotY * slotSize >= height) {
    std::cerr << "Texture Atlas Full! Cannot pack " << name << std::endl;
    return;
  }

  int slotX = nextSlotX;
  int slotY = nextSlotY;
  nextSlotX++;

  // Store Info
  TextureInfo info;
  info.slotX = slotX;
  info.slotY = slotY;

  // UVs
  // add small epsilon to avoid bleeding? Or rely on Nearest.
  // Nearest neighbor at patch boundaries can bleed if not careful.
  // Using padding is better, but here we just use strict coordinates.
  info.uMin = (float)(slotX * slotSize) / width;
  info.vMin = (float)(slotY * slotSize) / height;
  info.uMax = (float)((slotX + 1) * slotSize) / width;
  info.vMax = (float)((slotY + 1) * slotSize) / height;

  info.isAnimated = (frameCount > 1);
  info.frameCount = frameCount;
  info.frameTime = frameTime;

  textures[name] = info;
  // Also store by "filename" without ext?
  // Handled by Load() using stem().

  // Copy first frame to Atlas Data
  SetRegion(slotX * slotSize, slotY * slotSize, frameW, frameH, imgData,
            channels);

  // Handle Animation
  if (frameCount > 1) {
    AnimatedTexture anim;
    anim.name = name;
    anim.width = frameW;
    anim.height = frameH;
    anim.slotX = slotX;
    anim.slotY = slotY;
    anim.currentFrame = 0;
    anim.timer = 0.0f;
    // FrameTime is in Ticks (1/20s)
    anim.fps = 20 / (frameTime > 0 ? frameTime : 1);
    if (anim.fps <= 0)
      anim.fps = 1;

    // Copy all frames
    size_t totalSize = w * h * 4; // Assuming 4 channels conversion
    anim.frames.resize(totalSize);
    // If source was RGBA (4), copy directly.
    // We forced 4 channels in load.
    memcpy(anim.frames.data(), imgData, totalSize);

    animatedTextures.push_back(anim);
  }
}

void TextureAtlas::SetRegion(int x, int y, int w, int h,
                             const unsigned char *src, int channels) {
  // Copy row by row
  for (int row = 0; row < h; ++row) {
    int destY = y + row;
    if (destY >= height)
      break;

    int destIdx = (destY * width + x) * 4;
    int srcIdx = (row * w) * channels;

    // Copy pixels
    for (int col = 0; col < w; ++col) {
      if (x + col >= width)
        break;

      int d = destIdx + col * 4;
      int s = srcIdx + col * channels;

      data[d] = src[s];
      data[d + 1] = src[s + 1];
      data[d + 2] = src[s + 2];
      if (channels == 4)
        data[d + 3] = src[s + 3];
      else
        data[d + 3] = 255;
    }
  }
}

bool TextureAtlas::Update(float deltaTime) {
  bool anyUpdate = false;
  for (auto &anim : animatedTextures) {
    anim.timer += deltaTime;
    float distinctFrameTime = 1.0f / anim.fps;

    if (anim.timer >= distinctFrameTime) {
      anim.timer -= distinctFrameTime;
      anim.currentFrame = (anim.currentFrame + 1) %
                          (anim.frames.size() / (anim.width * anim.height * 4));

      // Update Atlas Data helper (Host side)
      // We need to update the GPU. The host data 'data' should also optionally
      // be updated if we re-upload everything? But we usually use SubImage.

      dirty = true;
      anyUpdate = true;
    }
  }
  return anyUpdate;
}

void TextureAtlas::UpdateTextureGPU(unsigned int textureID) {
  if (!dirty)
    return;

  glBindTexture(GL_TEXTURE_2D, textureID);

  for (const auto &anim : animatedTextures) {
    // Find offset in frames
    // total frames = height / frameHeight
    // frame data start = currentFrame * (w * h * 4)
    size_t frameSize = anim.width * anim.height * 4;

    // Safety check
    if ((anim.currentFrame + 1) * frameSize > anim.frames.size())
      continue;

    const unsigned char *frameData =
        anim.frames.data() + (anim.currentFrame * frameSize);

    glTexSubImage2D(GL_TEXTURE_2D, 0, anim.slotX * slotSize,
                    anim.slotY * slotSize, anim.width, anim.height, GL_RGBA,
                    GL_UNSIGNED_BYTE, frameData);
  }

  dirty = false;
}

bool TextureAtlas::GetTextureUV(const std::string &name, float &uMin,
                                float &vMin) const {
  auto it = textures.find(name);
  if (it != textures.end()) {
    uMin = it->second.uMin;
    vMin = it->second.vMin;
    return true;
  }
  return false;
}
