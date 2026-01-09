#ifndef BLOCK_LOADER_H
#define BLOCK_LOADER_H

#include "Block.h"
#include "BlockDefinition.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

class BlockLoader {
public:
  // Load all block definitions from a directory (recursive)
  static std::vector<Block *>
  loadFromDirectory(const std::filesystem::path &dir);

  // Load block definition(s) from a single JSON file
  static std::vector<Block *> loadFromFile(const std::filesystem::path &path);

private:
  // Parse JSON into BlockDefinition
  static BlockDef::BlockDefinition parseJSON(const nlohmann::json &j);

  struct VariantContext {
    std::string fullCode;
    std::unordered_map<std::string, std::string> variables;
  };

  // Expand variant groups into individual variant codes
  static std::vector<VariantContext>
  expandVariants(const BlockDef::BlockDefinition &def);

  // Create a Block instance from definition and variant code
  static Block *createBlockFromDefinition(const BlockDef::BlockDefinition &def,
                                          const VariantContext &ctx,
                                          block_id blockId);

  // Resolve property value based on variant using byType patterns
  template <typename T>
  static T resolveProperty(const T &defaultValue,
                           const std::unordered_map<std::string, T> &byTypeMap,
                           const std::string &variantCode) {
    // ... implementation unchanged (templated in header) ...
    // Find the most specific matching pattern
    std::string bestMatch;
    T bestValue = defaultValue;
    int bestMatchScore = -1;

    for (const auto &[pattern, value] : byTypeMap) {
      if (matchesPattern(pattern, variantCode)) {
        // Simple scoring: more specific patterns (fewer wildcards) score higher
        int score = 0;
        for (char c : pattern) {
          if (c != '*')
            score++;
        }
        if (score > bestMatchScore) {
          bestMatchScore = score;
          bestValue = value;
          bestMatch = pattern;
        }
      }
    }

    return bestValue;
  }

  // Pattern matching for byType wildcards
  static bool matchesPattern(const std::string &pattern,
                             const std::string &value);

  // Apply texture substitutions {wood} -> oak, etc.
  static std::string
  substituteVariables(const std::string &str, const std::string &variantCode,
                      const std::unordered_map<std::string, std::string> &vars);

  // Counter for auto-assigning block IDs
  static block_id nextBlockId;
};

#endif
