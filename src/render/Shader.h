#ifndef SHADER_H
#define SHADER_H

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader {
public:
  unsigned int ID;

  Shader(const std::filesystem::path &vertexPath,
         const std::filesystem::path &fragmentPath);
  void use();

  // Utility uniform functions
  void setBool(const std::string &name, bool value) const;
  void setInt(const std::string &name, int value) const;
  void setFloat(const std::string &name, float value) const;
  void setVec3(const std::string &name, const glm::vec3 &value) const;
  void setVec3(const std::string &name, float x, float y, float z) const;
  void setVec2(const std::string &name, const glm::vec2 &value) const;
  void setVec2(const std::string &name, float x, float y) const;
  void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
  void checkCompileErrors(unsigned int shader, std::string type);
};

#endif
