#include "split_utils.h"

#include <gdal_priv.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "dota_utils.h"
#include "loguru.hpp"
#include "path_utils.hpp"
#include "poly_iou.hpp"

using std::endl;
using std::list;
using std::string;
using std::vector;

string get_gdal_image_type(const string& file) {
  static const std::unordered_map<string, string> suffix2gdal{
      {{"png", "PNG"},
       {"bmp", "BMP"},
       {"JPG", "JPEG"},
       {"jpg", "JPEG"},
       {"tif", "GTiff"}}};
  const string& file_suffix = path::suffix(file);
  if (suffix2gdal.find(file_suffix) == suffix2gdal.end()) {
    return "";
  }
  const string& gdal_type = suffix2gdal.at(file_suffix);
  return gdal_type;
}

list<vector<size_t>> get_sliding_window(const content_t& info,
                                        const vector<int> sizes,
                                        const vector<int> gaps,
                                        const float& img_rate_thr) {
  const size_t& width = info.width;
  const size_t& height = info.height;
  list<vector<size_t>> windows;
  for (size_t i = 0; i < sizes.size(); i++) {
    const auto size = static_cast<size_t>(sizes[i]);
    const auto gap = static_cast<size_t>(gaps[i]);
    CHECK_F(size > gap, "invalid size gap pair [%ld %ld]", size, gap);
    const size_t step = size - gap;

    const size_t x_num =
        width <= size ? 1
                      : static_cast<size_t>(std::ceil(
                            static_cast<double>(width - size) / step + 1));

    vector<size_t> x_start(x_num, 0);
    for (size_t i = 0; i < x_num; i++) {
      x_start[i] = step * i;
    }
    if (x_start.size() > 1 && x_start.back() + size > width) {
      x_start.back() = width - size;
    }

    const size_t y_num =
        height <= size ? 1
                       : static_cast<size_t>(std::ceil(
                             static_cast<double>(height - size) / step + 1));

    vector<size_t> y_start(y_num, 0);
    for (size_t i = 0; i < y_num; i++) {
      y_start[i] = step * i;
    }
    if (y_start.size() > 1 && y_start.back() + size > height) {
      y_start.back() = height - size;
    }

    for (size_t i = 0; i < x_start.size(); i++) {
      for (size_t j = 0; j < y_start.size(); j++) {
        const size_t &x1 = x_start[i], &y1 = y_start[j], x2 = x_start[i] + size,
                     y2 = y_start[j] + size;
        const size_t _x1 = std::max(0UL, x1);
        const size_t _y1 = std::max(0UL, y1);
        const size_t _x2 = std::min(x2, width);
        const size_t _y2 = std::min(y2, height);
        float img_area = (_x2 - _x1) * (_y2 - _y1);
        float win_area = (x2 - x1) * (y2 - y1);
        float img_rate = img_area / win_area;
        if (img_rate < img_rate_thr) {
          continue;
        }
        windows.push_back(vector<size_t>{x1, y1, x2, y2});
      }
    }
  }
  return windows;
}

vector<vector<double>> obj_overlaps_iof(const list<vector<size_t>>& bboxes1,
                                        const vector<vector<double>>& bboxes2) {
  const auto rows = bboxes1.size();
  const auto cols = bboxes2.size();
  if (rows * cols == 0) {
    return vector<vector<double>>(0);
  }
  vector<vector<double>> iofs(rows, vector<double>(cols, 0));
  int i = 0;
  for (auto& tb : bboxes1) {
    for (size_t j = 0; j < cols; j++) {
      double tx = static_cast<double>(tb[0]), ty = static_cast<double>(tb[1]),
             tw = static_cast<double>(tb[2] - tb[0]),
             th = static_cast<double>(tb[3] - tb[1]);
      double bbox1[8]{tx,      ty,      tx + tw, ty,
                      tx + tw, ty + th, tx,      ty + th};  // 顺时针
      auto bbox2 = bboxes2[j].data();
      iofs[i][j] =
          std::single_poly_iou_rotated<double>(bbox2, bbox1, std::kIoF);
    }
    i++;
  }
  return iofs;
}

vector<ann_t> get_window_obj(const content_t& info,
                             const list<vector<size_t>> windows,
                             const float& iof_thr) {
  double eps = 1e-6;
  const auto& bboxes = info.ann.bboxes;
  const auto&& iofs = obj_overlaps_iof(windows, bboxes);
  vector<ann_t> window_anns;
  window_anns.reserve(windows.size());
  for (size_t i = 0; i < windows.size(); i++) {
    const auto& win_iofs = iofs[i];
    ann_t window_ann;
    for (size_t j = 0; j < win_iofs.size(); j++) {
      if (win_iofs[j] >= static_cast<double>(iof_thr)) {
        window_ann.bboxes.push_back(info.ann.bboxes[j]);
        window_ann.labels.push_back(info.ann.labels[j]);
        window_ann.diffs.push_back(info.ann.diffs[j]);
        window_ann.trunc.push_back(std::fabs(win_iofs[j] - 1) > eps);
      }
    }
    window_anns.push_back(window_ann);
  }
  return window_anns;
}

size_t crop_and_save_img(const content_t& info,
                         const list<vector<size_t>>& windows,
                         const vector<ann_t>& window_anns,
                         const string& img_dir, const bool& no_padding,
                         const vector<float>& padding_value,
                         const string& save_dir, const string& anno_dir,
                         const string& img_ext,
                         const float& ignore_empty_prob) {
  auto img_file = img_dir + info.filename;
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(img_file.c_str(), GA_ReadOnly));
  size_t i = 0;
  for (auto& window : windows) {
    auto& ann = window_anns[i];
    if (static_cast<float>(rand() % 100) / 100 < ignore_empty_prob &&
        ann.labels.empty()) {
      continue;
    }
    const auto& x_start = window[0];
    const auto& y_start = window[1];
    const auto& x_stop = window[2];
    const auto& y_stop = window[3];
    std::stringstream id_ss;
    id_ss << info.id << "__" << x_stop - x_start << "__" << x_start << "___"
          << y_start;
    const string& id = id_ss.str();
    // TODO: ignore empty patch
    auto& _bboxes = ann.bboxes;
    list<vector<double>> bboxes;
    {
      for (auto& _bbox : _bboxes) {
        vector<double> bbox(_bbox.size(), 0);
        for (int j = 0; j < static_cast<int>(_bbox.size()); j++) {
          bbox[j] = j % 2 == 0 ? _bbox[j] - x_start : _bbox[j] - y_start;
        }
        bboxes.push_back(bbox);
      }
    }

    const size_t& width = info.width;
    const size_t& height = info.height;
    const auto _x_stop = std::min(x_stop, width);
    const auto _y_stop = std::min(y_stop, height);
    const auto x_num = _x_stop - x_start;
    const auto y_num = _y_stop - y_start;

    const string& save_img_file = save_dir + id + img_ext;
    {
      const size_t img_height = y_stop - y_start;
      const size_t img_width = x_stop - x_start;
      const size_t _x_num = !no_padding ? img_width : x_num;
      const size_t _y_num = !no_padding ? img_height : y_num;

      const auto tmp_band = dataset->GetRasterBand(1);
      const auto data_type = tmp_band->GetRasterDataType();
      const size_t data_size = GDALGetDataTypeSizeBytes(data_type);
      const auto nchannels = dataset->GetRasterCount();

      const string& out_gdal_type = get_gdal_image_type(save_img_file);
      CHECK_F(!out_gdal_type.empty(), "unsupport type %s ",
              path::suffix(save_img_file).c_str());

      GDALDriver* mem_driver;
      mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
      CHECK_F(mem_driver != nullptr, "GetDriverByName \"MEM\": %s",
              CPLGetLastErrorMsg());
      GDALDataset* mem_dataset =
          mem_driver->Create("", _x_num, _y_num, nchannels, data_type, nullptr);

      GByte* buf = new GByte[_x_num * _y_num * data_size];
      for (int j = 1; j <= nchannels; j++) {
        auto src_band = dataset->GetRasterBand(j);  // RGB
        auto dst_band = mem_dataset->GetRasterBand(j);
        const int pi =
            padding_value.size() - (j - 1) % padding_value.size() - 1;
        memset(buf, static_cast<unsigned char>(padding_value[pi]),
               _x_num * _y_num * data_size);
        CPLErr ret;
        ret = src_band->RasterIO(GF_Read, x_start, y_start, x_num, y_num, buf,
                                 _x_num, _y_num, data_type, 0, 0);
        CHECK_F(ret < CE_Failure, "RasterIO %s: %s", info.filename.c_str(),
                CPLGetLastErrorMsg());
        ret = dst_band->RasterIO(GF_Write, 0, 0, _x_num, _y_num, buf, _x_num,
                                 _y_num, data_type, 0, 0);
        CHECK_F(ret < CE_Failure, "RasterIO %s: %s", info.filename.c_str(),
                CPLGetLastErrorMsg());
      }
      delete[] buf;

      GDALDriver* out_driver;
      out_driver =
          GetGDALDriverManager()->GetDriverByName(out_gdal_type.c_str());

      auto out_dataset = out_driver->CreateCopy(
          save_img_file.c_str(), mem_dataset, FALSE, nullptr, nullptr, nullptr);

      CHECK_F(out_dataset != nullptr, "CreateCopy %s: %s",
              save_img_file.c_str(), CPLGetLastErrorMsg());

      GDALClose(static_cast<GDALDatasetH>(mem_dataset));
      GDALClose(static_cast<GDALDatasetH>(out_dataset));
    }

    const string& save_ann_file = anno_dir + id + ".txt";
    std::ofstream output_file(save_ann_file);
    size_t j = 0;
    for (auto& bbox : bboxes) {
      auto outline = std::accumulate(
          bbox.begin(), bbox.end(), string(""),
          [](string& lhs, const int& rhs) {
            return lhs.empty() ? std::to_string(rhs)
                               : lhs + " " + std::to_string(rhs);
          });
      const char diff = !ann.trunc[j] ? ann.diffs[j] + '0' : '2';
      output_file << outline << " " << info.ann.labels[j] << " " << diff
                  << endl;
      j++;
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
                    const string& img_ext, const float& ignore_empty_prob,
                    const size_t& total, size_t& prog, std::mutex& lock) {
  srand(4096);

  auto& info = arguments.first;
  auto& img_dir = arguments.second;
  auto&& windows = get_sliding_window(info, sizes, gaps, img_rate_thr);
  auto&& window_anns = get_window_obj(info, windows, iof_thr);
  size_t num_patches = crop_and_save_img(info, windows, window_anns, img_dir,
                                         no_padding, padding_value, save_dir,
                                         anno_dir, img_ext, ignore_empty_prob);

  std::lock_guard lg(lock);
  prog += 1;
  LOG(INFO) << std::setiosflags(std::ios::fixed) << std::setprecision(2)
            << static_cast<float>(prog) / total * 100 << "%"
            << " " << prog << ":" << total << " - "
            << "filename: " << info.filename << " - "
            << "width: " << info.width << " - "
            << "height: " << info.height << " - "
            << "objects: " << info.ann.bboxes.size() << " - "
            << "patches: " << num_patches << endl;
  return num_patches;
}
