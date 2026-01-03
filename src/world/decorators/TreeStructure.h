#pragma once
#include "../../utils/MathUtils.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct Distribution {
  std::string dist = "none";
  float avg = 0.0f;
  float var = 0.0f;

  float Sample(std::mt19937 &rng) const {
    if (dist == "gaussian") {
      return MathUtils::SampleGaussian(rng, avg, var);
    } else if (dist == "inversegaussian") {
      return MathUtils::SampleInverseGaussian(rng, avg, var);
    } else if (dist == "triangle") {
      return MathUtils::SampleTriangle(rng, avg - var, avg + var, avg);
    } else if (dist == "uniform") {
      return MathUtils::SampleUniform(rng, avg - var, avg + var);
    }
    return avg; // "none" or unknown
  }
};

inline void from_json(const json &j, Distribution &d) {
  if (j.is_number()) {
    d.avg = j.get<float>();
    d.var = 0.0f;
    d.dist = "uniform";
    return;
  }

  bool hasDist = j.contains("dist");
  if (hasDist)
    d.dist = j.at("dist").get<std::string>();

  if (j.contains("avg"))
    d.avg = j.at("avg").get<float>();
  if (j.contains("var"))
    d.var = j.at("var").get<float>();

  // Implicit Uniform: If data is provided but no dist specified, assume uniform
  if (!hasDist && (j.contains("avg") || j.contains("var"))) {
    d.dist = "uniform";
  }
}

struct Evolution {
  MathUtils::TransformType transform = MathUtils::TransformType::NONE;
  float factor = 0.0f;

  float Apply(float value, float progress) const {
    return MathUtils::ApplyTransform(value, progress, transform, factor);
  }
};

inline void from_json(const json &j, Evolution &e) {
  if (j.contains("factor"))
    e.factor = j.at("factor").get<float>();
  std::string t = j.value("transform", "none");
  if (t == "linear")
    e.transform = MathUtils::TransformType::LINEAR;
  else if (t == "quadratic")
    e.transform = MathUtils::TransformType::QUADRATIC;
  else if (t == "sinus")
    e.transform = MathUtils::TransformType::SINUS;
  else
    e.transform = MathUtils::TransformType::NONE;
}

struct TreeSegment {
  // VS: Initial width multiplier (for trunks: size * widthMultiplier)
  float widthMultiplier = 1.0f;

  float widthLoss = 0.05f;      // VS default
  Distribution randomWidthLoss; // VS: Randomizes widthLoss per tree for variety

  float widthlossCurve =
      1.0f; // VS: dampening factor for width loss as branch thins
  float branchWidthLossMul = 1.0f; // Multiplier for child branch width loss
  float gravityDrag = 0.0f;
  Distribution dieAt; // VS default: NatFloat.createUniform(0.0002f, 0)

  float dx = 0.5f, // VS default for trunks
      dz = 0.5f;   // Offset from parent (for trunks relative to root)

  // VS: Trunk can have its own angles (for tilted/crooked trunks)
  // VS: Trunk can have its own angles (for tilted/crooked trunks)
  Distribution angleVert; // Default "none" (0) fits VS (Uniform 0,0)
  Distribution angleHori = {"uniform", 0.0f, 3.14159f}; // VS: Uniform(0, PI)

  Distribution branchStart = {"uniform", 0.7f, 0.0f};   // VS: Uniform(0.7, 0)
  Distribution branchSpacing = {"uniform", 0.3f, 0.0f}; // VS: Uniform(0.3, 0)
  Distribution branchVerticalAngle = {"uniform", 0.0f,
                                      3.14159f}; // VS: Uniform(0, PI)
  Distribution branchHorizontalAngle = {"uniform", 0.0f,
                                        3.14159f}; // VS: Uniform(0, PI)

  Distribution branchQuantity = {"uniform", 1.0f, 0.0f}; // VS: Uniform(1, 0)
  Evolution branchQuantityEvolve;

  Distribution branchWidthMultiplier = {"uniform", 0.0f,
                                        0.0f}; // VS: Uniform(0, 0)
  Evolution branchWidthMultiplierEvolve;
  Evolution angleVertEvolve;
  Evolution angleHoriEvolve; // VS: Horizontal angle evolution (spiraling)

  // VS: Specialty flags
  bool NoLogs = false; // VS default
  int segment = 0;     // For multi-segmented trunks (VS feature)

  // Inheritance not fully implemented in struct, handle at load time?
};

inline void from_json(const json &j, TreeSegment &s) {
  if (j.contains("widthMultiplier"))
    s.widthMultiplier = j.at("widthMultiplier").get<float>();
  if (j.contains("widthLoss"))
    s.widthLoss = j.at("widthLoss").get<float>();
  if (j.contains("randomWidthLoss"))
    s.randomWidthLoss = j.at("randomWidthLoss");
  if (j.contains("widthlossCurve"))
    s.widthlossCurve = j.at("widthlossCurve").get<float>();
  if (j.contains("branchWidthLossMul"))
    s.branchWidthLossMul = j.at("branchWidthLossMul").get<float>();
  if (j.contains("gravityDrag"))
    s.gravityDrag = j.at("gravityDrag").get<float>();
  if (j.contains("dieAt"))
    s.dieAt = j.at("dieAt");

  if (j.contains("dx"))
    s.dx = j.at("dx").get<float>();
  if (j.contains("dz"))
    s.dz = j.at("dz").get<float>();

  // VS: Trunk angles
  if (j.contains("angleVert"))
    s.angleVert = j.at("angleVert");
  if (j.contains("angleHori"))
    s.angleHori = j.at("angleHori");

  if (j.contains("branchStart"))
    s.branchStart = j.at("branchStart");
  if (j.contains("branchSpacing"))
    s.branchSpacing = j.at("branchSpacing");
  if (j.contains("branchVerticalAngle"))
    s.branchVerticalAngle = j.at("branchVerticalAngle");
  if (j.contains("branchHorizontalAngle"))
    s.branchHorizontalAngle = j.at("branchHorizontalAngle");

  if (j.contains("branchQuantity"))
    s.branchQuantity = j.at("branchQuantity");
  if (j.contains("branchQuantityEvolve"))
    s.branchQuantityEvolve = j.at("branchQuantityEvolve");

  if (j.contains("branchWidthMultiplier"))
    s.branchWidthMultiplier = j.at("branchWidthMultiplier");
  if (j.contains("branchWidthMultiplierEvolve"))
    s.branchWidthMultiplierEvolve = j.at("branchWidthMultiplierEvolve");

  if (j.contains("angleVertEvolve"))
    s.angleVertEvolve = j.at("angleVertEvolve");
  if (j.contains("angleHoriEvolve"))
    s.angleHoriEvolve = j.at("angleHoriEvolve");

  // VS: Specialty flags
  if (j.contains("NoLogs"))
    s.NoLogs = j.at("NoLogs").get<bool>();
  if (j.contains("segment"))
    s.segment = j.at("segment").get<int>();
}

struct TreeBlocks {
  std::string logBlockCode;
  std::string leavesBlockCode;
  std::string leavesBranchyBlockCode;
  std::string vinesBlockCode;
  std::string vinesEndBlockCode;
  std::string mossDecorCode;

  // VS: Trunk segments for multi-textured trunks (redwood pine)
  std::string trunkSegmentBase;
  std::vector<std::string> trunkSegmentVariants;
};

inline void from_json(const json &j, TreeBlocks &b) {
  if (j.contains("logBlockCode"))
    b.logBlockCode = j.at("logBlockCode").get<std::string>();
  if (j.contains("leavesBlockCode"))
    b.leavesBlockCode = j.at("leavesBlockCode").get<std::string>();
  if (j.contains("leavesBranchyBlockCode"))
    b.leavesBranchyBlockCode =
        j.at("leavesBranchyBlockCode").get<std::string>();
  if (j.contains("vinesBlockCode"))
    b.vinesBlockCode = j.at("vinesBlockCode").get<std::string>();
  if (j.contains("vinesEndBlockCode"))
    b.vinesEndBlockCode = j.at("vinesEndBlockCode").get<std::string>();
  if (j.contains("mossDecorCode"))
    b.mossDecorCode = j.at("mossDecorCode").get<std::string>();

  // VS: Trunk segment support
  if (j.contains("trunkSegmentBase"))
    b.trunkSegmentBase = j.at("trunkSegmentBase").get<std::string>();
  if (j.contains("trunkSegmentVariants"))
    b.trunkSegmentVariants =
        j.at("trunkSegmentVariants").get<std::vector<std::string>>();
}

struct TreeStructure {
  std::string treeWorldPropertyCode;
  float sizeMultiplier = 1.0f;
  Distribution sizeVar; // VS: Size variance for tree variety
  int yOffset = 0;

  std::vector<TreeSegment> trunks;
  std::vector<TreeSegment> branches; // Level 0 branches, Level 1 branches...
  TreeBlocks treeBlocks;
};

inline void from_json(const json &j, TreeStructure &t) {
  if (j.contains("treeWorldPropertyCode"))
    t.treeWorldPropertyCode = j.at("treeWorldPropertyCode").get<std::string>();
  if (j.contains("sizeMultiplier"))
    t.sizeMultiplier = j.at("sizeMultiplier").get<float>();
  if (j.contains("sizeVar"))
    t.sizeVar = j.at("sizeVar");
  if (j.contains("yOffset"))
    t.yOffset = j.at("yOffset").get<int>();

  if (j.contains("trunks"))
    t.trunks = j.at("trunks").get<std::vector<TreeSegment>>();
  if (j.contains("branches"))
    t.branches = j.at("branches").get<std::vector<TreeSegment>>();
  if (j.contains("treeBlocks"))
    t.treeBlocks = j.at("treeBlocks");
}
