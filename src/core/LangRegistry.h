#ifndef LANG_REGISTRY_H
#define LANG_REGISTRY_H

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class LangRegistry {
public:
  static LangRegistry &get() {
    static LangRegistry instance;
    return instance;
  }

  // Load a language file (e.g. assets/lang/en.json)
  void loadLanguage(const std::string &langCode,
                    const std::filesystem::path &path);

  // Load all language files in a directory
  void loadAll(const std::filesystem::path &dir);

  // Set the current active language
  void setLanguage(const std::string &langCode);

  // Get localized string, returns key if not found
  std::string get(const std::string &key) const;

  // Get localized string with simple placeholder substitution {0}, {1}, etc.
  std::string get(const std::string &key,
                  const std::vector<std::string> &args) const;

private:
  LangRegistry() : m_currentLang("en") {}

  std::string m_currentLang;
  // Map: langCode -> { key -> value }
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      m_languages;
};

// Shorthand for getting strings
inline std::string Lang(const std::string &key) {
  return LangRegistry::get().get(key);
}

#endif
