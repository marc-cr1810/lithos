#include "BlockLoader.h"
#include "../debug/Logger.h"
#include "BlockFactory.h"
#include "behaviors/BlockBehaviorFactory.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

block_id BlockLoader::nextBlockId = 100; // Start from 100 to avoid conflicts

std::vector<Block *>
BlockLoader::loadFromDirectory(const std::filesystem::path &dir) {
  std::vector<Block *> blocks;

  if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
    LOG_WARN("Block definition directory does not exist: {}", dir.string());
    return blocks;
  }

  // Recursively find all .json files
  for (const auto &entry : std::filesystem::recursive_directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      LOG_DEBUG("Loading block JSON: {}", entry.path().string());
      auto fileBlocks = loadFromFile(entry.path());
      blocks.insert(blocks.end(), fileBlocks.begin(), fileBlocks.end());
    }
  }

  LOG_INFO("Loaded {} blocks from JSON definitions", blocks.size());
  return blocks;
}

std::vector<Block *>
BlockLoader::loadFromFile(const std::filesystem::path &path) {
  std::vector<Block *> blocks;

  std::ifstream f(path);
  if (!f.is_open()) {
    LOG_ERROR("Failed to open block definition file: {}", path.string());
    return blocks;
  }

  json j;
  try {
    f >> j;
    LOG_DEBUG("  -> JSON parsed successfully");
  } catch (const std::exception &e) {
    LOG_ERROR("JSON parsing error in {}: {}", path.string(), e.what());
    return blocks;
  }

  try {
    LOG_DEBUG("  -> Parsing block definition...");
    BlockDef::BlockDefinition def = parseJSON(j);
    LOG_DEBUG("  -> Expanding variants...");
    std::vector<BlockLoader::VariantContext> variants = expandVariants(def);
    LOG_DEBUG("  -> Creating {} variant(s)...", variants.size());

    for (const auto &ctx : variants) {
      LOG_DEBUG("    -> Creating block for variant: {}", ctx.fullCode);
      Block *block = createBlockFromDefinition(def, ctx, nextBlockId++);
      if (block) {
        blocks.push_back(block);
        LOG_TRACE("Loaded block definition: {}", ctx.fullCode);
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR("Error creating blocks from {}: {}", path.string(), e.what());
  }

  return blocks;
}

BlockDef::BlockDefinition BlockLoader::parseJSON(const nlohmann::json &j) {
  BlockDef::BlockDefinition def;

  // Required: code
  if (j.contains("code")) {
    def.code = j.at("code").get<std::string>();
  }

  // id parsing removed
  if (j.contains("solid")) {
    def.isSolid = j.at("solid").get<bool>();
  }
  if (j.contains("opaque")) {
    def.isOpaque = j.at("opaque").get<bool>();
  }
  if (j.contains("randomTickable")) {
    def.isRandomTickable = j.at("randomTickable").get<bool>();
  }
  // idByType parsing removed

  // Optional: class
  if (j.contains("class")) {
    def.blockClass = j.at("class").get<std::string>();
  }

  // Variant groups
  if (j.contains("variantGroups")) {
    for (const auto &vg : j.at("variantGroups")) {
      BlockDef::VariantGroup group;
      if (vg.contains("code")) {
        group.code = vg.at("code").get<std::string>();
      }
      if (vg.contains("states")) {
        for (const auto &state : vg.at("states")) {
          group.states.push_back(state.get<std::string>());
        }
      }
      if (vg.contains("loadFromProperties")) {
        group.loadFromProperties =
            vg.at("loadFromProperties").get<std::string>();
      }
      def.variantGroups.push_back(group);
    }
  }

  // RenderLayer
  if (j.contains("renderLayer")) {
    def.renderLayer = j.at("renderLayer").get<std::string>();
  }

  // DrawType
  if (j.contains("drawType")) {
    def.drawType = j.at("drawType").get<std::string>();
  }
  if (j.contains("drawTypeByType")) {
    for (auto it = j.at("drawTypeByType").begin();
         it != j.at("drawTypeByType").end(); ++it) {
      def.drawTypeByType[it.key()] = it.value().get<std::string>();
    }
  }

  // Block material
  if (j.contains("blockMaterial")) {
    def.blockMaterial = j.at("blockMaterial").get<std::string>();
  }

  // Resistance
  if (j.contains("resistance")) {
    def.resistance = j.at("resistance").get<float>();
  }
  if (j.contains("resistanceByType")) {
    for (auto it = j.at("resistanceByType").begin();
         it != j.at("resistanceByType").end(); ++it) {
      def.resistanceByType[it.key()] = it.value().get<float>();
    }
  }

  // Creative Inventory
  if (j.contains("creativeInventory")) {
    for (auto it = j.at("creativeInventory").begin();
         it != j.at("creativeInventory").end(); ++it) {
      std::vector<std::string> patterns;
      if (it.value().is_array()) {
        for (const auto &pattern : it.value()) {
          patterns.push_back(pattern.get<std::string>());
        }
      } else {
        patterns.push_back(it.value().get<std::string>());
      }
      def.creativeInventory[it.key()] = patterns;
    }
  }

  // Replaceable
  if (j.contains("replaceable")) {
    def.replaceable = j.at("replaceable").get<int>();
  }

  // Light
  if (j.contains("lightAbsorption")) {
    def.lightAbsorption = j.at("lightAbsorption").get<int>();
  }
  if (j.contains("emission")) {
    def.emission = j.at("emission").get<int>();
  }

  // Textures
  if (j.contains("textures")) {
    for (auto it = j.at("textures").begin(); it != j.at("textures").end();
         ++it) {
      BlockDef::TextureDef texDef;
      const auto &value = it.value();
      if (value.is_object()) {
        if (value.contains("base")) {
          texDef.base = value.at("base").get<std::string>();
        }
        if (value.contains("rotation")) {
          texDef.rotation = value.at("rotation").get<int>();
        }
        if (value.contains("overlays")) {
          for (const auto &ov : value.at("overlays")) {
            texDef.overlays.push_back(ov.get<std::string>());
          }
        }
      } else if (value.is_string()) {
        texDef.base = value.get<std::string>();
      }
      def.textures[it.key()] = texDef;
    }
  }

  // Textures by type
  if (j.contains("texturesByType")) {
    for (auto it = j.at("texturesByType").begin();
         it != j.at("texturesByType").end(); ++it) {
      std::unordered_map<std::string, BlockDef::TextureDef> texMap;
      for (auto valIt = it.value().begin(); valIt != it.value().end();
           ++valIt) {
        BlockDef::TextureDef texDef;
        const auto &value = valIt.value();
        if (value.is_object()) {
          if (value.contains("base")) {
            texDef.base = value.at("base").get<std::string>();
          }
          if (value.contains("rotation")) {
            texDef.rotation = value.at("rotation").get<int>();
          }
        } else if (value.is_string()) {
          texDef.base = value.get<std::string>();
        }
        texMap[valIt.key()] = texDef;
      }
      def.texturesByType[it.key()] = texMap;
    }
  }

  // Sounds
  if (j.contains("sounds")) {
    const auto &s = j.at("sounds");
    if (s.contains("place")) {
      def.sounds.place = s.at("place").get<std::string>();
    }
    if (s.contains("walk")) {
      def.sounds.walk = s.at("walk").get<std::string>();
    }
    if (s.contains("hit")) {
      def.sounds.hit = s.at("hit").get<std::string>();
    }
    if (s.contains("breakSound")) {
      def.sounds.breakSound = s.at("breakSound").get<std::string>();
    }
  }

  // Shape
  if (j.contains("shape")) {
    const auto &s = j.at("shape");
    if (s.is_object()) {
      if (s.contains("base")) {
        def.shape.base = s.at("base").get<std::string>();
      }
      if (s.contains("rotateX")) {
        def.shape.rotateX = s.at("rotateX").get<int>();
      }
      if (s.contains("rotateY")) {
        def.shape.rotateY = s.at("rotateY").get<int>();
      }
      if (s.contains("rotateZ")) {
        def.shape.rotateZ = s.at("rotateZ").get<int>();
      }
    }
  }

  // Behaviors
  if (j.contains("behaviors")) {
    for (const auto &behavior : j.at("behaviors")) {
      BlockDef::BehaviorDef behaviorDef;
      if (behavior.contains("name")) {
        behaviorDef.name = behavior.at("name").get<std::string>();
      }
      if (behavior.contains("properties")) {
        behaviorDef.properties = behavior.at("properties");
      }
      def.behaviors.push_back(behaviorDef);
    }
  }

  // Behaviors By Type
  if (j.contains("behaviorsByType")) {
    for (auto it = j.at("behaviorsByType").begin();
         it != j.at("behaviorsByType").end(); ++it) {
      std::vector<BlockDef::BehaviorDef> behaviorList;
      for (const auto &behavior : it.value()) {
        BlockDef::BehaviorDef behaviorDef;
        if (behavior.contains("name")) {
          behaviorDef.name = behavior.at("name").get<std::string>();
        }
        if (behavior.contains("properties")) {
          behaviorDef.properties = behavior.at("properties");
        }
        behaviorList.push_back(behaviorDef);
      }
      def.behaviorsByType[it.key()] = behaviorList;
    }
  }

  // Attributes
  if (j.contains("attributes")) {
    def.attributes = j.at("attributes");
  }
  if (j.contains("attributesByType")) {
    for (const auto &item : j.at("attributesByType").items()) {
      def.attributesByType[item.key()] = item.value();
    }
  }

  // Climate Color Map By Type
  if (j.contains("climateColorMapByType")) {
    for (const auto &item : j.at("climateColorMapByType").items()) {
      if (item.value().is_string()) {
        def.climateColorMapByType[item.key()] = item.value().get<std::string>();
      }
    }
  }

  // Tint Target
  if (j.contains("tintTarget")) {
    def.tintTarget = j.at("tintTarget").get<std::string>();
  }
  if (j.contains("tintTargetByType")) {
    for (const auto &item : j.at("tintTargetByType").items()) {
      if (item.value().is_string()) {
        def.tintTargetByType[item.key()] = item.value().get<std::string>();
      }
    }
  }

  return def;
}

std::vector<BlockLoader::VariantContext>
BlockLoader::expandVariants(const BlockDef::BlockDefinition &def) {
  std::vector<VariantContext> variants;

  if (def.variantGroups.empty()) {
    // No variants, just use the base code
    variants.push_back({def.code, {}});
    return variants;
  }

  // Start with base code
  variants.push_back({def.code, {}});

  // Expand each variant group
  for (const auto &group : def.variantGroups) {
    std::vector<VariantContext> newVariants;
    for (const auto &existingCtx : variants) {
      for (const auto &state : group.states) {
        VariantContext newCtx = existingCtx;
        newCtx.fullCode = existingCtx.fullCode + "_" + state;
        newCtx.variables[group.code] = state;
        newVariants.push_back(newCtx);
      }
    }
    variants = newVariants;
  }

  return variants;
}

Block *
BlockLoader::createBlockFromDefinition(const BlockDef::BlockDefinition &def,
                                       const VariantContext &ctx,
                                       block_id blockId) {
  const std::string &variantCode = ctx.fullCode;

  // Determine drawtype for this variant
  std::string drawType =
      resolveProperty(def.drawType, def.drawTypeByType, variantCode);

  // Determine resistance for this variant
  float resistance =
      resolveProperty(def.resistance, def.resistanceByType, variantCode);

  // Determine attributes for this variant
  nlohmann::json attributes =
      resolveProperty(def.attributes, def.attributesByType, variantCode);

  // Create appropriate block type based on drawtype or behaviors
  Block *block = nullptr;
  // Check for specific behaviors that determine block class
  bool isFalling = false;
  bool isLiquid = false;
  for (const auto &behavior : def.behaviors) {
    if (behavior.name == "UnstableFalling" || behavior.name == "Falling") {
      isFalling = true;
    } else if (behavior.name == "Liquid") {
      isLiquid = true;
    }
  }

  // Determine ID: always use the dynamically assigned blockId
  block_id finalId = blockId;

  // Create block based on class or inferred type
  // Determine Class Name
  std::string className = "SolidBlock"; // Default

  if (!def.blockClass.empty()) {
    className = def.blockClass;
  } else {
    // Inference Logic for backward compatibility or concise JSON
    if (isFalling) {
      className = "FallingBlock";
    } else if (isLiquid) {
      if (variantCode == "water")
        className = "WaterBlock";
      else if (variantCode == "lava")
        className = "LavaBlock";
      else
        className = "LiquidBlock";
    } else if (drawType == "cross") {
      if (variantCode.find("leaves") != std::string::npos)
        className = "LeavesBlock";
      else
        className = "PlantBlock";
    } else if (drawType == "cube" || drawType == "json") {
      className = "SolidBlock";
    }
  }

  block =
      BlockFactory::getInstance().createBlock(className, finalId, variantCode);

  if (!block) {
    LOG_ERROR("Failed to create block instance for {}", variantCode);
    return nullptr;
  }

  // Set resource ID
  LOG_DEBUG("      -> Setting resource ID");
  block->setResourceId("lithos:" + variantCode);

  // Set attributes
  block->setAttributes(attributes);

  // Store which creative tabs this block belongs to (for later registration)
  // We don't call BlockRegistry::getInstance() here to avoid deadlock
  LOG_DEBUG("      -> Storing creative tab memberships (count: {})",
            def.creativeInventory.size());
  for (const auto &[tabCode, patterns] : def.creativeInventory) {
    for (const auto &pattern : patterns) {
      if (matchesPattern(pattern, variantCode)) {
        block->addCreativeTab(tabCode);
        break; // Only add once per tab
      }
    }
  }

  // Apply textures
  // First check for texturesByType
  bool foundTypeTextures = false;
  for (const auto &[pattern, texMap] : def.texturesByType) {
    if (matchesPattern(pattern, variantCode)) {
      foundTypeTextures = true;
      // 1. Apply 'all' first if it exists in this pattern
      if (texMap.count("all")) {
        const auto &texDef = texMap.at("all");
        std::string texturePath =
            substituteVariables(texDef.base, variantCode, ctx.variables);
        block->setTexture(texturePath);
        for (int i = 0; i < 6; ++i) {
          block->setFaceRotation(i, texDef.rotation);
          for (size_t k = 0; k < texDef.overlays.size(); ++k) {
            std::string overlayPath = substituteVariables(
                texDef.overlays[k], variantCode, ctx.variables);
            block->setOverlayTexture(i, overlayPath);
          }
        }
      }

      // 2. Apply specific faces for this pattern
      for (const auto &[face, texDef] : texMap) {
        if (face == "all")
          continue;

        std::string texturePath =
            substituteVariables(texDef.base, variantCode, ctx.variables);

        // Function to apply texture and overlays for a face index
        auto applyTex = [&](int faceIdx) {
          block->setTexture(faceIdx, texturePath);
          block->setFaceRotation(faceIdx, texDef.rotation);
          for (size_t i = 0; i < texDef.overlays.size(); ++i) {
            std::string overlayPath = substituteVariables(
                texDef.overlays[i], variantCode, ctx.variables);
            block->setOverlayTexture(faceIdx, overlayPath);
          }
        };

        if (face == "north") {
          applyTex(1);
        } else if (face == "south") {
          applyTex(0);
        } else if (face == "east") {
          applyTex(3);
        } else if (face == "west") {
          applyTex(2);
        } else if (face == "up") {
          applyTex(4);
        } else if (face == "down") {
          applyTex(5);
        } else if (face == "horizontals") {
          for (int i = 0; i < 4; ++i)
            applyTex(i);
        } else if (face == "verticals") {
          applyTex(4);
          applyTex(5);
        }
      }
      break; // Use first matching pattern
    }
  }

  // If no type-specific textures found, use default textures
  if (!foundTypeTextures) {
    // 1. Apply 'all' first if it exists
    if (def.textures.count("all")) {
      const auto &texDef = def.textures.at("all");
      std::string texturePath =
          substituteVariables(texDef.base, variantCode, ctx.variables);
      block->setTexture(texturePath);
      for (int i = 0; i < 6; ++i) {
        block->setFaceRotation(i, texDef.rotation);
        for (size_t k = 0; k < texDef.overlays.size(); ++k) {
          std::string overlayPath = substituteVariables(
              texDef.overlays[k], variantCode, ctx.variables);
          block->setOverlayTexture(i, overlayPath);
        }
      }
    }

    // 2. Apply specific faces
    for (const auto &[face, texDef] : def.textures) {
      if (face == "all")
        continue;

      std::string texturePath =
          substituteVariables(texDef.base, variantCode, ctx.variables);

      auto applyTex = [&](int faceIdx) {
        block->setTexture(faceIdx, texturePath);
        block->setFaceRotation(faceIdx, texDef.rotation);
        for (size_t i = 0; i < texDef.overlays.size(); ++i) {
          std::string overlayPath = substituteVariables(
              texDef.overlays[i], variantCode, ctx.variables);
          block->setOverlayTexture(faceIdx, overlayPath);
        }
      };

      if (face == "north") {
        applyTex(1);
      } else if (face == "south") {
        applyTex(0);
      } else if (face == "east") {
        applyTex(3);
      } else if (face == "west") {
        applyTex(2);
      } else if (face == "up") {
        applyTex(4);
      } else if (face == "down") {
        applyTex(5);
      } else if (face == "horizontals") {
        for (int i = 0; i < 4; ++i)
          applyTex(i);
      } else if (face == "verticals") {
        applyTex(4);
        applyTex(5);
      }
    }
  }

  // Set render shape
  if (drawType == "cross") {
    block->setRenderShape(Block::RenderShape::CROSS);
    // Implied properties for cross shape:
    if (def.renderLayer.empty()) {
      block->setRenderLayer(Block::RenderLayer::CUTOUT);
    }
    // Cross shapes are never opaque cubes
    block->setOpaque(false);
  } else if (drawType == "cube") {
    block->setRenderShape(Block::RenderShape::CUBE);
  } else if (drawType == "json" && !def.shape.base.empty()) {
    // Load custom model if specified
    // This would require model path resolution
  }

  // Set render layer
  if (def.renderLayer == "cutout") {
    block->setRenderLayer(Block::RenderLayer::CUTOUT);
    block->setOpaque(false); // Default to non-opaque for cutout
  } else if (def.renderLayer == "transparent") {
    block->setRenderLayer(Block::RenderLayer::TRANSPARENT);
    block->setOpaque(false); // Default to non-opaque for transparent
  } else {
    block->setRenderLayer(Block::RenderLayer::OPAQUE);
  }

  // Set opacity based on drawtype/material (Can override defaults from
  // renderLayer if explicitly set in JSON)
  block->setResistance(resistance);
  // Only override isOpaque if it was explicitly present in JSON?
  // Current logic: def.isOpaque defaults to true.
  // If JSON had "opaque": false, def.isOpaque is false.
  // But if JSON didn't have "opaque", def.isOpaque is true.
  // We want renderLayer="transparent" to imply opaque=false unless specified
  // otherwise. BUT def.isOpaque is already parsed. Issue: We don't know if
  // "opaque" was present in JSON or default. FIX: use the fact that we set
  // opaque=false above, then OR it with the explicit setting? No. Let's rely on
  // manual "opaque": false in JSON if needed, OR just trust renderLayer? User
  // wants "Cant the engine just figure it out based on 'opaque'?". User asked
  // "Why have a renderLayer option?". So if I use renderLayer, I should
  // probably enforce it. If renderLayer is transparent, it CANNOT be opaque.
  if (block->getRenderLayer() != Block::RenderLayer::OPAQUE) {
    block->setOpaque(false);
  } else {
    block->setOpaque(def.isOpaque);
  }

  block->setSolid(def.isSolid);
  block->setReplaceable(def.replaceable > 0);
  block->setEmission(def.emission);
  block->setRandomTickable(def.isRandomTickable);
  // The following line was moved to be inside the function scope.
  // block->setOpaque(false); // This line was removed from here.

  // Parse Climate Color Map
  // Parse Climate Color Map
  std::string climateMap =
      resolveProperty<std::string>("", def.climateColorMapByType, variantCode);

  if (!climateMap.empty()) {
    block->setClimateColorMap(climateMap);

    // Resolve tintTarget from definition
    std::string targetStr =
        resolveProperty(def.tintTarget, def.tintTargetByType, variantCode);

    // Check attributes override
    if (attributes.contains("tintTarget")) {
      targetStr = attributes["tintTarget"].get<std::string>();
    } else if (attributes.contains("tintOverlayOnly")) {
      // Backwards compatibility
      if (attributes["tintOverlayOnly"].get<bool>())
        targetStr = "overlay";
    }

    Block::TintTarget target = Block::TintTarget::All;
    if (targetStr == "none")
      target = Block::TintTarget::None;
    else if (targetStr == "base")
      target = Block::TintTarget::Base;
    else if (targetStr == "overlay")
      target = Block::TintTarget::Overlay;

    block->setTintTarget(target);
  }

  // Attach Behaviors
  auto behaviors =
      resolveProperty(def.behaviors, def.behaviorsByType, variantCode);
  for (const auto &bDef : behaviors) {
    auto behavior =
        BlockBehaviorFactory::getInstance().createBehavior(bDef.name, block);

    if (behavior) {
      behavior->onLoaded(bDef.properties);
      block->addBehavior(behavior);

      // Special case property setting (TODO: Move to properties?)
      // Check if behavior implies random ticking
      // Ideally behavior should set this flag on the block in constructor or
      // onLoaded But BlockBehavior doesn't have easy mutable access to Block
      // flags? Actually it has 'block' pointer. Let's rely on behavior setting
      // logic (e.g. Growth sets it).
    } else {
      LOG_ERROR("BlockLoader: Unknown behavior '{}' for block '{}'", bDef.name,
                block->getName());
    }
  }

  return block;
}

bool BlockLoader::matchesPattern(const std::string &pattern,
                                 const std::string &value) {
  // Simple glob matching support for '*'
  size_t starPos = pattern.find('*');
  if (starPos == std::string::npos) {
    return pattern == value;
  }

  std::string prefix = pattern.substr(0, starPos);
  std::string suffix = pattern.substr(starPos + 1);

  if (value.length() < prefix.length() + suffix.length()) {
    return false;
  }

  bool prefixMatch = value.compare(0, prefix.length(), prefix) == 0;
  bool suffixMatch = value.compare(value.length() - suffix.length(),
                                   suffix.length(), suffix) == 0;

  return prefixMatch && suffixMatch;
}

std::string BlockLoader::substituteVariables(
    const std::string &str, const std::string &variantCode,
    const std::unordered_map<std::string, std::string> &vars) {
  std::string result = str;

  // 1. Use specific variables if available
  if (!vars.empty()) {
    for (const auto &[key, value] : vars) {
      std::string placeholder = "{" + key + "}";
      size_t pos = 0;
      while ((pos = result.find(placeholder, pos)) != std::string::npos) {
        result.replace(pos, placeholder.length(), value);
        pos += value.length();
      }
    }
  } else {
    // 2. Fallback: Simple placeholder substitution (old behavior)
    // Determine variant value (part after first underscore)
    size_t underscore = variantCode.find('_');
    if (underscore != std::string::npos) {
      std::string variantVal = variantCode.substr(underscore + 1);

      // Replace {wood}, {start}, {whatever} with the variant value
      size_t start = result.find('{');
      while (start != std::string::npos) {
        size_t end = result.find('}', start);
        if (end != std::string::npos) {
          result.replace(start, end - start + 1, variantVal);
          start = result.find('{', start + variantVal.length());
        } else {
          break;
        }
      }
    }
  }
  return result;
}
