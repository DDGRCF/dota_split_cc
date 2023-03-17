#include "split_utils.h"

#include <gdal_priv.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <numeric>
#include <sstream>
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
        windows.push_back(vector<int>{x1, y1, x2, y2});
      }
    }
  }
  return windows;
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
    for (size_t j = 0; j < cols; j++) {
      float tx = static_cast<float>(tb[0]), ty = static_cast<float>(tb[1]),
            tw = static_cast<float>(tb[2] - tb[0]),
            th = static_cast<float>(tb[3] - tb[1]);
      float bbox1[8]{tx,      ty,      tx + tw, ty,
                     tx + tw, ty + th, tx,      ty + th};  // 顺时针
      auto bbox2 = bboxes2[j].data();
      iofs[i][j] = std::single_poly_iou_rotated<float>(bbox2, bbox1, std::kIoF);
    }
    i++;
  }
  return iofs;
}

vector<ann_t> get_window_obj(const content_t& info, list<vector<int>> windows,
                             const float& iof_thr) {
  float eps = 1e-5;  // TODO:
  auto& bboxes = info.ann.bboxes;
  auto&& iofs = obj_overlaps_iof(windows, bboxes);
  vector<ann_t> window_anns;
  window_anns.reserve(windows.size());
  for (size_t i = 0; i < windows.size(); i++) {
    auto& win_iofs = iofs[i];
    ann_t window_ann;
    for (size_t j = 0; j < win_iofs.size(); j++) {
      if (win_iofs[j] >= iof_thr) {
        window_ann.bboxes.push_back(info.ann.bboxes[j]);
        window_ann.labels.push_back(info.ann.labels[j]);
        window_ann.diffs.push_back(info.ann.diffs[j]);
        window_ann.trunc.push_back(std::fabs(win_iofs[j] - 1) > eps);
      }
    }
  }
  return window_anns;
}

size_t crop_and_save_img(const content_t& info,
                         const list<vector<int>>& windows,
                         const vector<ann_t>& window_anns,
                         const string& img_dir, const bool& no_padding,
                         const vector<float>& padding_value,
                         const string& save_dir, const string& anno_dir,
                         const string& img_ext) {
  auto img_file = img_dir + info.filename;
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(img_file.c_str(), GA_ReadOnly));
  size_t i = 0;
  for (auto& window : windows) {
    const int& x_start = window[0];
    const int& y_start = window[1];
    const int& x_stop = window[0];
    const int& y_stop = window[1];
    std::stringstream id_ss;
    id_ss << info.id << "__" << x_stop - x_start << "__" << x_start << "___"
          << y_start;
    const string& id = id_ss.str();
    // TODO: ignore empty patch
    auto& ann = window_anns[i];
    auto& _bboxes = ann.bboxes;
    vector<vector<float>> bboxes(_bboxes.size(), vector<float>(8, 0));
    std::transform(_bboxes.begin(), _bboxes.end(), bboxes.begin(),
                   [&x_start, &y_start](const vector<float>& bbox) {
                     vector<float> new_bbox(8, 0);
                     for (int i = 0; i < static_cast<int>(bbox.size()); i++) {
                       new_bbox[i] =
                           i % 2 == 0 ? bbox[i] - x_start : bbox[i] - y_start;
                     }
                     return new_bbox;
                   });
    const auto& width = info.width;
    const auto& height = info.height;
    auto _x_stop = x_stop > width ? width : x_stop;
    auto _y_stop = y_stop > height ? height : y_stop;
    auto x_num = _x_stop - x_start;
    auto y_num = _y_stop - y_start;

    const string& save_img_file = save_dir + id + img_ext;
    {
      auto img_height = y_stop - y_start;
      auto img_width = x_stop - y_start;

      GDALDriver* driver;
      driver = GetGDALDriverManager()->GetDriverByName(
          dataset->GetDriver()->GetDescription());
      auto tmp_band = dataset->GetRasterBand(1);
      auto data_type = tmp_band->GetRasterDataType();
      auto out_dataset =
          driver->Create(save_img_file.c_str(), x_num, y_num,
                         dataset->GetRasterCount(), data_type, nullptr);
      for (int i = 0; i <= dataset->GetRasterCount(); i++) {
        GDALRasterBand* src_band = dataset->GetRasterBand(i);
        GDALRasterBand* dst_band = out_dataset->GetRasterBand(i);
        auto _x_num = !no_padding ? std::max(x_num, img_width) : x_num;
        auto _y_num = !no_padding ? std::max(y_num, img_height) : y_num;
        unsigned char* buf = new unsigned char[_x_num * _y_num];
        memset(buf, static_cast<int>(padding_value[i % padding_value.size()]),
               _x_num * _y_num);

        CPLErr ret;
        ret = src_band->RasterIO(GF_Read, x_start, y_start, x_num, y_num, buf,
                                 _x_num, _y_num, data_type, 0, 0);
        if (ret > CE_Warning) {
          LOG(INFO) << "RasterIO " << CPLGetLastErrorMsg() << endl;
          exit(1);
        }
        ret = dst_band->RasterIO(GF_Write, 0, 0, _x_num, _y_num, buf, _x_num,
                                 _y_num, data_type, 0, 0);
        if (ret > CE_Warning) {
          LOG(INFO) << "RasterIO " << CPLGetLastErrorMsg() << endl;
          exit(1);
        }
        delete[] buf;
      }
      GDALClose(static_cast<GDALDatasetH>(out_dataset));
    }

    const string& save_ann_file = anno_dir + id + ".txt";
    std::ofstream output_file(save_ann_file);
    for (size_t i = 0; i < bboxes.size(); i++) {
      auto outline = std::accumulate(
          bboxes[i].begin(), bboxes[i].end(), string(""),
          [](string& lhs, const int& rhs) {
            return lhs.empty() ? std::to_string(rhs)
                               : lhs + " " + std::to_string(rhs);
          });
      auto diff = std::to_string(!ann.trunc[i] ? ann.diffs[i] + '0' : '2');
      outline += diff;
      output_file << outline << endl;
    }
    i++;
  }
  GDALClose(static_cast<GDALDatasetH>(dataset));
  return i;
}

size_t single_split(const std::pair<content_t, string>& arguments,
                    const vector<int>& sizes, const vector<int>& gaps,
                    const float& img_rate_thr, const float& iof_thr,
                    const bool& no_padding, const vector<float>& padding_value,
                    const string& save_dir, const string& anno_dir,
                    const string& img_ext, const size_t& total, size_t& prog,
                    std::mutex& lock) {
  auto& info = arguments.first;
  auto& img_dir = arguments.second;
  auto&& windows = get_sliding_window(info, sizes, gaps, img_rate_thr);
  auto&& window_anns = get_window_obj(info, windows, iof_thr);
  size_t num_patches =
      crop_and_save_img(info, windows, window_anns, img_dir, no_padding,
                        padding_value, save_dir, anno_dir, img_ext);
  std::lock_guard lg(lock);
  prog += 1;
  LOG(INFO) << std::setprecision(2) << static_cast<float>(prog) / total << " "
            << prog << ":" << total << " - "
            << "filename: " << info.filename << " - "
            << "width: " << info.width << " - "
            << "height: " << info.height << " - "
            << "objects: " << info.ann.bboxes.size() << " - "
            << "patches: " << num_patches << endl;
  return num_patches;
}
