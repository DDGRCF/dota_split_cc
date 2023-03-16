#ifndef PATH_UTILS_HPP_
#define PATH_UTILS_HPP_

#include <dirent.h>
#include <errno.h>
#include <glob.h>

#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace path {
static inline std::string path_join(
    const std::initializer_list<std::string> paths) {
  return std::accumulate(paths.begin(), paths.end(), std::string(""),
                         [](const std::string& x, const std::string& y) {
                           return x.empty() ? y : x + y;
                         });
}

template <typename... Args>
inline std::string join(Args&&... args) {
  static_assert(((std::is_same_v<std::decay_t<Args>, std::string> ||
                  std::is_same_v<std::decay_t<Args>, const char*>) ||
                 ...),
                "Args is not basic_string");
  return path_join({args...});
}

inline std::string basename(const std::string& path) {
  auto left = path.rfind('/');
  if (left == std::string::npos) {
    return path;
  }
  return path.substr(left + 1);
}

inline std::string dirname(const std::string& path) {
  auto left = path.rfind('/');
  if (left == std::string::npos) {
    return path;
  }
  return path.substr(0, left);
}

inline std::string suffix(const std::string& path) {
  auto left = path.rfind('.');
  if (left == std::string::npos) {
    return "";
  }
  return path.substr(left + 1);
}

inline std::string stem(const std::string& path) {
  auto left = path.rfind('/');
  auto right = path.rfind('.');
  if (left == std::string::npos && right != std::string::npos) {
    return path.substr(0, right);
  } else if (left != std::string::npos && right == std::string::npos) {
    return path.substr(left + 1);
  } else if (left != std::string::npos && right != std::string::npos &&
             right > left) {
    return path.substr(left + 1, right - left - 1);
  } else {
    return path;
  }
}

inline bool is_exist(const fs::path& path) { return fs::exists(path); }

inline bool is_dir(const fs::path& path) {
  if (!is_exist(path)) {
    errno = ENOENT;
    return false;
  }
  return fs::is_directory(path);
}
inline bool is_file(const fs::path& path) {
  if (!is_exist(path)) {
    errno = ENOENT;
    return false;
  }
  return fs::is_regular_file(path);
}

inline std::vector<std::string> glob(const std::string& path) {
  std::vector<std::string> res{};
  const std::string& dirname = path::dirname(path);
  if (!path::is_exist(dirname)) {
    return res;
  }
  int ret;
  glob_t globbuf{0};
  do {
    ret = ::glob(path.c_str(), GLOB_NOSORT, nullptr, &globbuf);
    if (ret != 0) {
      break;
    }
    res.reserve(globbuf.gl_pathc);
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
      res.emplace_back(globbuf.gl_pathv[i]);
    }
  } while (0);
  globfree(&globbuf);
  return res;
}

}  // namespace path

#endif