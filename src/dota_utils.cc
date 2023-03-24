#include "dota_utils.h"

#include <gdal_priv.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <loguru.hpp>
#include <sstream>
#include <string>
#include <unordered_set>

#include "path_utils.hpp"
#include "string_utils.hpp"
#include "threadpool.hpp"

using std::endl;
using std::string;
using std::vector;

enum ContentType {
  kUnSupport = -2,
  kParseError = -1,
  kEmpty = 0,
};

content_t _load_dota_txt(const string& txt_file) {
  float gsd = kEmpty;
  vector<vector<double>> bboxes;
  vector<string> labels;
  vector<int> diffs;
  if (!txt_file.empty()) {
    do {
      if (!path::is_file(txt_file)) {
        LOG(INFO) << "can't find " << txt_file << ", treated as empty txt_file"
                  << endl;
        break;
      }

      std::ifstream input_file(txt_file);
      auto lines_count = std::count(std::istreambuf_iterator<char>(input_file),
                                    std::istreambuf_iterator<char>(), '\n') +
                         1;  // 统计文件行数，最后一行统计不到

      bboxes.reserve(lines_count);
      labels.reserve(lines_count);
      diffs.reserve(lines_count);

      input_file.seekg(std::ios::beg);

      string line;
      while (std::getline(input_file, line)) {  // 读入消耗掉换行符
        auto left = line.rfind('\r');
        if (left != std::string::npos) {
          line = line.substr(0, left);
        }
        if (line.empty()) {
          continue;
        }
        if (str::starts_with(line, "gsd")) {
          auto pos = line.find(':');
          if (pos != string::npos) {
            auto gsd_str = line.substr(pos + 1);
            try {
              gsd = std::stof(gsd_str);
            } catch (std::invalid_argument& invalid) {
              gsd = kParseError;
            }
          }
          continue;
        } else if (str::starts_with(line, "imagesource") ||
                   str::starts_with(line, "NAN")) {
          continue;
        }
        auto line_split = str::split(line);
        if (line_split.size() >= 9) {
          vector<double> bbox(8, 0);
          std::transform(line_split.begin(), line_split.begin() + 8,
                         bbox.begin(),
                         [](string valstr) { return std::stof(valstr); });
          bboxes.push_back(bbox);
          labels.push_back(line_split[8]);
          diffs.push_back(line_split.size() == 10 ? std::stoi(line_split[9])
                                                  : 0);
        }
      }
    } while (0);
  }
  return content_t{gsd, "", "", 0, 0, {bboxes, labels, diffs}};
}

content_t _load_dota_single(const string& img_file, const string& ann_dir) {
  static const std::unordered_set<string> support_ext{"jpg", "JPG", "png",
                                                      "tif", "bmp"};
  auto img_id = path::stem(img_file);
  auto ext = path::suffix(img_file);
  if (support_ext.find(ext) == support_ext.end()) {
    return content_t{kUnSupport};
  }
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(img_file.c_str(), GA_ReadOnly));
  int width = dataset->GetRasterXSize();
  int height = dataset->GetRasterYSize();
  string txt_file = ann_dir.empty() ? "" : (ann_dir + img_id + ".txt");
  content_t content = _load_dota_txt(txt_file);
  content.width = width;
  content.height = height;
  content.filename = path::basename(img_file);
  content.id = img_id;
  GDALClose(static_cast<GDALDatasetH>(dataset));
  return content;
}

vector<content_t> load_dota(const string& img_dir, const string& ann_dir,
                            const int& nthread) {
  LOG(INFO) << "starting loading the dataset information." << endl;
  auto start_time = std::chrono::system_clock::now();
  auto _load_func = [&ann_dir](const string& img_file) {
    return _load_dota_single(img_file, ann_dir);
  };
  auto path_set = path::glob(img_dir + "*");
  vector<content_t> contents;
  contents.reserve(path_set.size());
  if (nthread > 1) {
    std::threadpool pool(nthread);  // try openmp
    auto contents_future = pool.map_container(_load_func, path_set);
    for (auto& content_future : contents_future) {
      contents.push_back(content_future.get());
    }
    // contents.resize(path_set.size()); omp_set_num_threads(nthread); //
    // -fopenmp #pragma omp parallel for
    //     for (int i = 0; i < path_set.size(); i++) {
    //       contents[i] = _load_func(path_set[i]);
    //     }
  } else {
    std::transform(path_set.begin(), path_set.end(),
                   std::back_inserter(contents), _load_func);
  }
  contents.erase(std::remove_if(contents.begin(), contents.end(),
                                [](const content_t& content) {
                                  return content.gsd == kUnSupport;
                                }),
                 contents.end());
  auto end_time = std::chrono::system_clock::now();
  LOG(INFO) << "finishing loading dataset, get " << contents.size()
            << " images,"
            << " using "
            << std::chrono::duration_cast<std::chrono::seconds>(end_time -
                                                                start_time)
                   .count()
            << "s." << endl;
  return contents;
}