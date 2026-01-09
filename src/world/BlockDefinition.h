#ifndef BLOCK_DEFINITION_H
#define BLOCK_DEFINITION_H

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace BlockDef {

// Texture definition for a face or layer
struct TextureDef {
  std::string base;
  int rotation = 0;
  std::vector<std::string> overlays;
};

// Sound definitions
struct SoundsDef {
  std::string place;
  std::string walk;
  std::string hit;
  std::string breakSound;
};

// Variant group for expanding multiple block variants
struct VariantGroup {
  std::string code;
  std::vector<std::string> states;
  std::string loadFromProperties; // For external property files (future)
};

// Shape/Model definition
struct ShapeDef {
  std::string base;
  int rotateX = 0;
  int rotateY = 0;
  int rotateZ = 0;
};

// Collision/Selection box
struct BoundingBox {
  float x1 = 0.0f, y1 = 0.0f, z1 = 0.0f;
  float x2 = 1.0f, y2 = 1.0f, z2 = 1.0f;
};

// Block behavior definition
struct BehaviorDef {
  std::string name;
  nlohmann::json properties;
};

// Main block definition structure
struct BlockDefinition {
  std::string code;
  // id field removed (use idByType)
  std::string blockClass; // Optional custom C++ class name
  std::vector<VariantGroup> variantGroups;

  // Rendering
  std::string drawType = "cube";
  std::string renderLayer = "opaque"; // New field
  ShapeDef shape;
  std::unordered_map<std::string, TextureDef> textures;
  std::unordered_map<std::string, std::unordered_map<std::string, TextureDef>>
      texturesByType;

  // Material & Physics
  std::string blockMaterial = "Stone";
  float resistance = 1.0f;
  int replaceable = 0;
  int lightAbsorption = 15;
  int emission = 0;

  // Sounds
  SoundsDef sounds;

  // Collision & Selection
  BoundingBox collisionBox;
  BoundingBox selectionBox;
  bool hasCustomCollisionBox = false;
  bool hasCustomSelectionBox = false;

  // Rendering flags
  bool isSolid = true;
  bool isOpaque = true;
  std::unordered_map<std::string, bool> sideOpaque; // per-face opacity
  std::unordered_map<std::string, bool> sideSolid;  // per-face solidity
  std::string tintTarget = "all"; // "all", "none", "base", "overlay"
  bool isRandomTickable = false;

  // Behaviors
  std::vector<BehaviorDef> behaviors;
  std::unordered_map<std::string, std::vector<BehaviorDef>> behaviorsByType;

  // Creative Inventory
  std::unordered_map<std::string, std::vector<std::string>> creativeInventory;

  // Attributes (Arbitrary JSON data)
  nlohmann::json attributes;

  // Conditional properties (byType patterns)
  std::unordered_map<std::string, std::string> drawTypeByType;
  // idByType removed
  std::unordered_map<std::string, float> resistanceByType;
  std::unordered_map<std::string, ShapeDef> shapeByType;
  std::unordered_map<std::string, std::unordered_map<std::string, bool>>
      sideOpaqueByType;
  std::unordered_map<std::string, std::unordered_map<std::string, bool>>
      sideSolidByType;
  std::unordered_map<std::string, BoundingBox> collisionBoxByType;
  std::unordered_map<std::string, BoundingBox> selectionBoxByType;
  std::unordered_map<std::string, nlohmann::json> attributesByType;
  std::unordered_map<std::string, std::string> climateColorMapByType;
  std::unordered_map<std::string, std::string> tintTargetByType;
};

} // namespace BlockDef

#endif
