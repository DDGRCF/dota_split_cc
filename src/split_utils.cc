#include "split_utils.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "box_type.h"
#include "dota_utils.h"
#include "loguru.hpp"

using std::endl;
using std::list;
using std::string;
using std::vector;

list<HBox> get_sliding_window(const content_t& info, const vector<int> sizes,
                              const vector<int> gaps,
                              const float& img_rate_thr) {
  const float eps = 0.01;
  const int& width = info.width;
  const int& height = info.height;
  list<HBox> windows;
  for (size_t i = 0; i < sizes.size(); i++) {
    auto size = sizes[i];
    auto gap = gaps[i];
    CHECK_F(size > gap, "invalid size gap pair [%d %d]", size, gap);
    auto step = size - gap;

    int x_num =
        width <= size
            ? 1
            : static_cast<int>(std::ceil((width - size) * 1. / step + 1));
    vector<int> x_start(x_num, 0);
    for (int i = 0; i < x_num; i++) {
      x_start[i] = step * i;
    }
    if (x_start.size() > 1 && x_start.back() + size > width) {
      x_start.back() = width - size;
    }

    int y_num =
        height <= size
            ? 1
            : static_cast<int>(std::ceil((height - size) / step * 1. + 1));
    vector<int> y_start(x_num, 0);
    for (int i = 0; i < y_num; i++) {
      y_start[i] = step * i;
    }
    if (y_start.size() > 1 && y_start.back() + size > height) {
      y_start.back() = height - size;
    }
    for (int i = 0; i < static_cast<int>(x_start.size()); i++) {
      for (int j = 0; j < static_cast<int>(y_start.size()); j++) {
        int x1 = x_start[i], y1 = y_start[j], x2 = x_start[i] + size,
            y2 = y_start[j] + size;
        auto _x2 = std::min(x2, width);
        auto _y2 = std::min(y2, height);
        auto img_area = (_x2 - x1) * (_y2 - y1);
        auto win_area = (x2 - x1) * (y2 - y1);
        float img_rate = img_area / win_area;
        if (img_rate < img_rate_thr) {
          continue;
        }
        auto hbox = HBox{x1, y1, x2, y2};
        windows.push_back(hbox);
      }
    }
  }
  return windows;
}

void get_window_obj(const content_t& info, list<HBox> windows,
                    const float& iof_thr) {}

void single_split(const std::pair<content_t, string>& arguments,
                  const vector<int>& sizes, const vector<int>& gaps,
                  const float& img_rate_thr, const float& iof_thr,
                  const bool& no_padding, const vector<float>& padding_value,
                  const string& save_dir, const string& anno_dir,
                  const string& img_ext, const std::atomic<int>& prog) {}
