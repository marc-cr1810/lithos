#include "LangRegistry.h"
#include "../debug/Logger.h"
#include <fstream>

using json = nlohmann::json;

void LangRegistry::loadLanguage(const std::string &langCode,
                                const std::filesystem::path &path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    LOG_ERROR("Failed to open language file: {}", path.string());
    return;
  }

  json j;
  try {
    f >> j;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON parsing error in {}: {}", path.string(), e.what());
    return;
  }

  auto &langMap = m_languages[langCode];
  for (auto &[key, value] : j.items()) {
    if (value.is_string()) {
      langMap[key] = value.get<std::string>();
    }
  }

  LOG_INFO("Loaded language {} from {} ({} strings)", langCode, path.string(),
           langMap.size());
}

void LangRegistry::loadAll(const std::filesystem::path &dir) {
  if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
    LOG_WARN("Language directory does not exist: {}", dir.string());
    return;
  }

  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      std::string langCode = entry.path().stem().string();
      loadLanguage(langCode, entry.path());
    }
  }
}

void LangRegistry::setLanguage(const std::string &langCode) {
  m_currentLang = langCode;
}

std::string LangRegistry::get(const std::string &key) const {
  auto it = m_languages.find(m_currentLang);
  if (it != m_languages.end()) {
    auto keyIt = it->second.find(key);
    if (keyIt != it->second.end()) {
      return keyIt->second;
    }
  }

  // Fallback to English if current isn't en
  if (m_currentLang != "en") {
    auto enIt = m_languages.find("en");
    if (enIt != m_languages.end()) {
      auto keyIt = enIt->second.find(key);
      if (keyIt != enIt->second.end()) {
        return keyIt->second;
      }
    }
  }

  return key; // Return key as fallback
}

std::string LangRegistry::get(const std::string &key,
                              const std::vector<std::string> &args) const {
  std::string str = get(key);
  for (size_t i = 0; i < args.size(); ++i) {
    std::string placeholder = "{" + std::to_string(i) + "}";
    size_t pos = 0;
    while ((pos = str.find(placeholder, pos)) != std::string::npos) {
      str.replace(pos, placeholder.length(), args[i]);
      pos += args[i].length();
    }
  }
  return str;
}
