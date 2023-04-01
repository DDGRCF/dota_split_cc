#ifndef PATH_UTILS_HPP_
#define PATH_UTILS_HPP_

#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace path {

static inline std::string
path_join(const std::initializer_list<std::string> paths) {
  return std::accumulate(paths.begin(), paths.end(), std::string(""),
                         [](const std::string &x, const std::string &y) {
                           return x.empty() ? y : x + y;
                         });
}

template <typename... Args> inline std::string join(Args &&... args) {
  // static_assert(
  //     ((std::is_same<typename std::decay<Args>::type, std::string>::value ||
  //       std::is_same<typename std::decay<Args>::type, const char*>::value) ||
  //      ...),
  //     "Args is not basic_string"); // only support in c++17
  int dummy[] = {(std::is_convertible<Args, std::string>::value, 0)...};
  (void)dummy;
  return path_join({args...});
}

inline std::string basename(const std::string &path) {
  auto left = path.rfind('/');
  if (left == std::string::npos) {
    return path;
  }
  return path.substr(left + 1);
}

inline std::string dirname(const std::string &path) {
  auto left = path.rfind('/');
  if (left == std::string::npos) {
    return path;
  }
  return path.substr(0, left);
}

inline std::string suffix(const std::string &path) {
  auto left = path.rfind('.');
  if (left == std::string::npos) {
    return "";
  }
  return path.substr(left + 1);
}

inline std::string stem(const std::string &path) {
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

inline bool is_exist(const std::string &path) {
  struct stat statbuf;
  int ret = stat(path.c_str(), &statbuf);
  if (ret == -1) {
    return false;
  }
  return true;
}

inline bool is_dir(const std::string &path) {
  if (!is_exist(path)) {
    return false;
  }
  struct stat statbuf;
  stat(path.c_str(), &statbuf);
  if (!S_ISDIR(statbuf.st_mode)) {
    return false;
  }
  return true;
}

inline bool is_file(const std::string &path) {
  if (!is_exist(path)) {
    return false;
  }
  struct stat statbuf;
  stat(path.c_str(), &statbuf);
  if (!S_ISREG(statbuf.st_mode)) {
    return false;
  }
  return true;
}

inline std::vector<std::string> glob(const std::string &path,
                                     const bool &sort_by_size = true) {
  std::vector<std::string> res{};
  const std::string &dirname = path::dirname(path);
  auto cmp_func = [](const std::string &f1, const std::string &f2) {
    struct stat stat1, stat2;
    if (stat(f1.c_str(), &stat1) != 0 || stat(f2.c_str(), &stat2) != 0) {
      return false;
    }
    return stat1.st_size < stat2.st_size;
  };

  if (!path::is_exist(dirname)) {
    return res;
  }

  int ret;
  glob_t globbuf{0};
  do {
    ret = ::glob(path.c_str(), GLOB_TILDE, nullptr, &globbuf);
    if (ret != 0) {
      break;
    }
    res.reserve(globbuf.gl_pathc);
    for (size_t i = 0; i < globbuf.gl_pathc; i++) {
      res.emplace_back(globbuf.gl_pathv[i]);
    }
    if (sort_by_size) {
      std::sort(res.begin(), res.end(), cmp_func);
    }
  } while (0);
  globfree(&globbuf);
  return res;
}

} // namespace path

#endif
