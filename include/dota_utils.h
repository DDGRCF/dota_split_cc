#ifndef DOTA_UTILS_H_
#define DOTA_UTILS_H_

#include <string>
#include <vector>

typedef struct {
  float gsd;
  std::string filename;
  std::string id;
  int width;
  int height;
  typedef struct {
    std::vector<std::vector<float>> bboxes;
    std::vector<int> labels;
    std::vector<int> diffs;
  } ann_t;
  ann_t ann;
} content_t;

std::vector<content_t> load_dota(const std::string& img_dir,
                                 const std::string& ann_dir,
                                 const int& nthread = 10);
#endif