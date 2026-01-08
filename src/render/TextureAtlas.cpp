
#include "TextureAtlas.h"
#include "../debug/Logger.h"
#include <GL/glew.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

// STB Image (Already implemented in Texture.cpp, but we need definitions if we
// want to use functions?) Actually stb_image implementation logic is strictly
// in one file. We just need the header.
#include "../vendor/stb_image.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "../vendor/stb_rect_pack.h"

namespace fs = std::filesystem;

TextureAtlas::TextureAtlas(int width, int height)
    : width(width), height(height), dirty(false) {
  // Initialize transparent black
  data.resize(width * height * 4, 0);
}

TextureAtlas::~TextureAtlas() {}

void TextureAtlas::Load(const std::string &directory) {
  if (!fs::exists(directory)) {
    LOG_RESOURCE_ERROR("TextureAtlas Error: Directory not found {}", directory);
    return;
  }

  LOG_RESOURCE_INFO("Loading textures from {}...", directory);
  fs::path baseDir(directory);

  for (const auto &entry : fs::recursive_directory_iterator(directory)) {
    if (entry.path().extension() == ".png") {
      // Use generic path processing
      std::string path = entry.path().string();
      std::string filename = entry.path().filename().string();
      std::string name =
          fs::relative(entry.path(), baseDir).replace_extension("").string();
      std::replace(name.begin(), name.end(), '\\', '/');

      int w, h, c;
      stbi_set_flip_vertically_on_load(true);
      unsigned char *img = stbi_load(path.c_str(), &w, &h, &c, 4);
      if (img) {
        int frameTime = 1;
        bool animated = false;

        // Check JSON for metadata
        std::string jsonPath = path + ".json";
        if (fs::exists(jsonPath)) {
          std::ifstream f(jsonPath);
          if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            if (content.find("\"frametime\"") != std::string::npos) {
              // Very basic parsing
              size_t pos = content.find("\"frametime\"");
              size_t colon = content.find(':', pos);
              if (colon != std::string::npos) {
                try {
                  frameTime = std::stoi(content.substr(colon + 1));
                  animated = true;
                } catch (...) {
                }
              }
            } else if (content.find("\"animation\"") != std::string::npos) {
              animated = true;
            }
          }
        }

        // Auto-detect strip
        if (h > w && h % w == 0 && (h / w) > 1) {
          if (!animated) {
            animated = true;
            frameTime = 20;
          }
        }

        int frames = 1;
        if (animated && w > 0)
          frames = h / w;

        // Queue it
        PackTexture(name, img, w, h, 4, frames, frameTime);

        stbi_image_free(img);
      } else {
        LOG_RESOURCE_ERROR("Failed to load texture: {} Reason: {}", filename,
                           stbi_failure_reason());
      }
    }
  }

  // Now execute packing
  if (pendingTextures.empty())
    return;

  // Prepare rects
  std::vector<stbrp_rect> rects;
  rects.reserve(pendingTextures.size());

  for (size_t i = 0; i < pendingTextures.size(); ++i) {
    stbrp_rect r;
    r.id = (int)i;
    r.w = pendingTextures[i].w;
    r.h = pendingTextures[i].h /
          pendingTextures[i].frames; // Pack single frame height
    r.was_packed = 0;
    rects.push_back(r);
  }

  // Pack
  stbrp_context context;
  std::vector<stbrp_node> nodes(width);
  stbrp_init_target(&context, width, height, nodes.data(), nodes.size());
  stbrp_pack_rects(&context, rects.data(), rects.size());

  // Process results
  for (const auto &r : rects) {
    auto &pt = pendingTextures[r.id];
    if (r.was_packed) {
      // Upload first frame to atlas
      SetRegion(r.x, r.y, r.w, r.h, pt.data.data(), pt.channels);

      TextureInfo info;
      info.uMin = (float)r.x / width;
      info.vMin = (float)r.y / height;
      info.uMax = (float)(r.x + r.w) / width;
      info.vMax = (float)(r.y + r.h) / height;
      info.slotX = r.x;
      info.slotY = r.y;
      info.isAnimated = pt.isAnimated;
      info.frameCount = pt.frames;
      info.frameTime = pt.frameTime;

      textures[pt.name] = info;

      if (pt.isAnimated) {
        AnimatedTexture anim;
        anim.name = pt.name;
        anim.width = r.w;
        anim.height = r.h;
        anim.slotX = r.x;
        anim.slotY = r.y;
        anim.currentFrame = 0;
        anim.timer = 0.0f;
        anim.fps = 20 / (pt.frameTime > 0 ? pt.frameTime : 1);
        if (anim.fps <= 0)
          anim.fps = 1;

        anim.frames = pt.data; // Copy full data
        animatedTextures.push_back(anim);
      }
    } else {
      LOG_RESOURCE_ERROR("TextureAtlas Full! Failed to pack '{}'", pt.name);
    }
  }

  LOG_RESOURCE_INFO("Texture Atlas Built. {} textures packed.",
                    textures.size());
  // Clear pending to free memory
  pendingTextures.clear();
}

void TextureAtlas::PackTexture(const std::string &name, unsigned char *imgData,
                               int w, int h, int channels, int frameCount,
                               int frameTime) {
  PendingTexture pt;
  pt.name = name;
  pt.w = w;
  pt.h = h;
  pt.channels = channels;
  pt.frames = frameCount;
  pt.frameTime = frameTime;
  pt.isAnimated = (frameCount > 1);

  // Copy data
  size_t size = w * h * channels;
  pt.data.resize(size);
  memcpy(pt.data.data(), imgData, size);

  pendingTextures.push_back(pt);
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

    glTexSubImage2D(GL_TEXTURE_2D, 0, anim.slotX, anim.slotY, anim.width,
                    anim.height, GL_RGBA, GL_UNSIGNED_BYTE, frameData);
  }

  dirty = false;
}

bool TextureAtlas::GetTextureUV(const std::string &name, float &uMin,
                                float &vMin, float &uMax, float &vMax) const {
  auto it = textures.find(name);
  if (it != textures.end()) {
    uMin = it->second.uMin;
    vMin = it->second.vMin;
    uMax = it->second.uMax;
    vMax = it->second.vMax;
    return true;
  }
  return false;
}

const TextureInfo *TextureAtlas::GetTextureInfo(const std::string &name) const {
  auto it = textures.find(name);
  if (it != textures.end()) {
    return &it->second;
  }
  return nullptr;
}
