#include "split_utils.h"

#include <gdal_priv.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "dota_utils.h"
#include "loguru.hpp"
#include "path_utils.hpp"
#include "poly_iou.hpp"

using std::endl;
using std::list;
using std::string;
using std::vector;

list<vector<int>> get_sliding_window(const content_t& info,
                                     const vector<int> sizes,
                                     const vector<int> gaps,
                                     const float& img_rate_thr) {
  const int& width = info.width;
  const int& height = info.height;
  list<vector<int>> windows;
  for (size_t i = 0; i < sizes.size(); i++) {
    auto size = sizes[i];
    auto gap = gaps[i];
    CHECK_F(size > gap, "invalid size gap pair [%d %d]", size, gap);
    auto step = size - gap;

    int x_num = width <= size
                    ? 1
                    : static_cast<int>(std::ceil(
                          static_cast<float>(width - size) / step + 1));

    vector<int> x_start(x_num, 0);
    for (int i = 0; i < x_num; i++) {
      x_start[i] = step * i;
    }
    if (x_start.size() > 1 && x_start.back() + size > width) {
      x_start.back() = width - size;
    }

    int y_num = height <= size
                    ? 1
                    : static_cast<int>(std::ceil(
                          static_cast<float>(height - size) / step + 1));

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
        float img_area = (_x2 - x1) * (_y2 - y1);
        float win_area = (x2 - x1) * (y2 - y1);
        float img_rate = img_area / win_area;
        if (img_rate < img_rate_thr) {
          continue;
        }
        windows.emplace_back(x1, y1, x2, y2);
      }
    }
  }
  return windows;
}

vector<ann_t> get_window_obj(const content_t& info, list<vector<int>> windows,
                             const float& iof_thr) {
  float eps = 1e-5;  // TODO:
  auto& bboxes = info.ann.bboxes;
  auto&& iofs = obj_overlaps_iof(windows, bboxes);
  vector<ann_t> window_anns;
  window_anns.reserve(windows.size());
  int i = 0;
  for (auto& window : windows) {
    auto& win_iofs = iofs[i];
    ann_t window_ann;
    // window_ann.bboxes.reserve(win_iofs.size());
    // window_ann.labels.reserve(win_iofs.size());
    for (int j = 0; j < win_iofs.size(); j++) {
      if (win_iofs[j] >= iof_thr) {
        window_ann.bboxes.push_back(info.ann.bboxes[j]);
        window_ann.labels.push_back(info.ann.labels[j]);
        window_ann.diffs.push_back(info.ann.diffs[j]);
        window_ann.turnc.push_back(std::fabs(win_iofs[j] - 1) > eps);
      }
    }
    i++;
  }
  return window_anns;
}

vector<vector<float>> obj_overlaps_iof(const list<vector<int>>& bboxes1,
                                       const vector<vector<float>>& bboxes2) {
  auto rows = bboxes1.size();
  auto cols = bboxes2.size();
  if (rows * cols == 0) {
    return vector<vector<float>>(0);
  }
  vector<vector<float>> iofs(rows, vector<float>(cols, 0));
  int i = 0;
  for (auto& tb : bboxes1) {
    for (int j = 0; j < cols; j++) {
      int tx = tb[0], ty = tb[1], tw = tb[2] - tb[0], th = tb[3] - tb[1];
      float bbox1[8]{tx,      ty,      tx + tw, ty,
                     tx + tw, ty + th, tx,      ty + th};  // 顺时针
      auto bbox2 = bboxes2[j].data();
      iofs[i][j] = std::single_poly_iou_rotated<float>(bbox2, bbox1, std::kIoF);
    }
    i++;
  }
  return iofs;
}

void crop_and_save_img(const content_t& info, const list<vector<int>>& windows,
                       const string& img_dir, const bool& no_padding,
                       const float& padding_value, const string& save_dir,
                       const string& anno_dir, const string& img_ext) {
  auto img_file = img_dir + info.filename;
  GDALDataset* dataset = static_cast<GDALDataset*>(GDALOpen(img_file.c_str()));
  int i = 0;
  for (auto& window : windows) {
  }
}

void single_split(const std::pair<content_t, string>& arguments,
                  const vector<int>& sizes, const vector<int>& gaps,
                  const float& img_rate_thr, const float& iof_thr,
                  const bool& no_padding, const vector<float>& padding_value,
                  const string& save_dir, const string& anno_dir,
                  const string& img_ext, const std::atomic<int>& prog) {}
