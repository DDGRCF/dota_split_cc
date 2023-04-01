#ifndef STRING_UTILS_HPP_
#define STRING_UTILS_HPP_

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace str {
inline bool starts_with(const std::string &str, const std::string &prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

inline std::vector<std::string> split(const std::string &line,
                                      const char &delim = ' ') {
  std::stringstream ss(line);
  std::vector<std::string> res;
  std::string word;
  while (std::getline(ss, word, delim)) {
    res.push_back(word);
  }
  return res;
}

inline std::string tolower(const std::string &line) {
  std::string new_line;
  new_line.reserve(line.size());
  std::transform(line.begin(), line.end(), std::back_inserter(new_line),
                 ::tolower);
  return new_line;
}

} // namespace str

#endif
