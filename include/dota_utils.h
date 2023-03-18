#ifndef DOTA_UTILS_H_
#define DOTA_UTILS_H_

#include <string>
#include <vector>

typedef struct {
  std::vector<std::vector<double>> bboxes;
  std::vector<std::string> labels;
  std::vector<int> diffs;
  std::vector<bool> trunc;
} ann_t;

typedef struct {
  float gsd;
  std::string filename;
  std::string id;
  size_t width;
  size_t height;
  ann_t ann;
} content_t;

std::vector<content_t> load_dota(const std::string& img_dir,
                                 const std::string& ann_dir,
                                 const int& nthread = 10);
#endif