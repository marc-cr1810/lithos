#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct TextureInfo {
  float uMin, vMin;
  float uMax, vMax;
  int slotX, slotY;
  bool isAnimated;
  int frameCount;
  int frameTime; // ticks or changes
};

struct AnimatedTexture {
  std::string name;
  int currentFrame;
  float timer;
  int fps;
  std::vector<unsigned char> frames; // All frames data sequentially
  int width, height;                 // of one frame
  int slotX, slotY;
};

class TextureAtlas {
public:
  TextureAtlas(int width, int height, int slotSize);
  ~TextureAtlas();

  // Load from directory
  void Load(const std::string &directory);

  // Updates animated textures
  // Returns true if any texture was updated (requires re-bind/subimage)
  bool Update(float deltaTime);

  // Bind/Upload logic helper?
  // For now, the main thread handles uploading the whole texture or we can add
  // helper here. Actually, we need the Texture ID to call glTexSubImage2D.
  void UpdateTextureGPU(unsigned int textureID);

  // Access raw data (for initial glTexImage2D)
  unsigned char *GetData() { return data.data(); }
  int GetWidth() const { return width; }
  int GetHeight() const { return height; }

  // Query
  bool GetTextureUV(const std::string &name, float &uMin, float &vMin) const;

private:
  int width;
  int height;
  int slotSize;
  std::vector<unsigned char> data;

  std::unordered_map<std::string, TextureInfo> textures;
  std::vector<AnimatedTexture> animatedTextures;

  // Dirty flag for animation updates
  bool dirty;
  // We track which slots are dirty to optimize?
  // For now simpler to just track if we need to interact with GL.

  // Helper
  void PackTexture(const std::string &name, unsigned char *imgData, int w,
                   int h, int channels, int frameCount = 1, int frameTime = 1);
  void SetRegion(int x, int y, int w, int h, const unsigned char *src,
                 int channels);

  // Simple grid packer state
  int nextSlotX;
  int nextSlotY;
};

#endif
