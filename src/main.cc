#include <fcntl.h>
#include <gdal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <string>
#include <vector>

#include "dota_utils.h"
#include "json.hpp"
#include "loguru.hpp"
#include "path_utils.hpp"
#include "split_utils.h"
#include "threadpool.hpp"

using json = nlohmann::json;
using std::endl;
using std::string;
using std::vector;

json parse_json(int argc, char **argv) {
  if (argc != 2) {
    LOG(ERROR) << "repect the number of input parameters is " << 2
               << " but get " << argc << std::endl;
    exit(1);
  }
  string json_file_path(argv[1]);
  std::ifstream json_file(json_file_path);
  json data = json::parse(json_file, nullptr, true, true);

  const string &&save_dir = data.at("save_dir");
  int ret = mkdir(save_dir.c_str(), 0774);
  CHECK_F(ret != -1, "mkdir %s: %s", save_dir.c_str(), strerror(errno));

  const string log_dir = save_dir + "splitting.log";
  loguru::add_file(log_dir.c_str(), loguru::Append, loguru::Verbosity_MAX);

  auto &&gaps = data.at("gaps");
  auto &&sizes = data.at("sizes");
  CHECK_F(gaps.size() == sizes.size(),
          "the sizes of gaps:%ld and sizes:%ld are not same", gaps.size(),
          sizes.size());

  auto &&img_dirs = data.at("img_dirs");
  auto &&ann_dirs = data.at("ann_dirs");
  if (!ann_dirs.is_null()) {
    CHECK_F(img_dirs.size() == ann_dirs.size(),
            "the sizes of img_dirs:%ld and ann_dirs:%ld are not same",
            img_dirs.size(), ann_dirs.size());
  }

  return data;
}

void deal(const json &configs) {
  auto &&rates = configs.at("rates");

  auto &&_sizes = configs.at("sizes");
  auto &&_gaps = configs.at("gaps");
  const int rows = static_cast<int>(rates.size());
  const int cols = static_cast<int>(_sizes.size());

  vector<int> sizes(rows * cols, 0);
  vector<int> gaps(rows * cols, 0);

  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      sizes[i * cols + j] = _sizes[j].get<int>() / rates[i].get<float>();
      gaps[i * cols + j] = _gaps[j].get<int>() / rates[i].get<float>();
    }
  }
  const string save_dir = configs.at("save_dir");

  auto &&save_imgs = save_dir + "images/";
  auto &&save_files = save_dir + "annfiles/";

  auto &&img_dirs = configs.at("img_dirs");
  auto &&ann_dirs =
      configs.at("ann_dirs").is_null() ? json::array() : configs.at("ann_dirs");

  int ret = mkdir(save_imgs.c_str(), 0774);
  CHECK_F(ret != -1, "mkdir %s: %s", save_imgs.c_str(), strerror(errno));

  if (!ann_dirs.empty()) {
    ret = mkdir(save_files.c_str(), 0774);
    CHECK_F(ret != -1, "mkdir %s: %s", save_files.c_str(), strerror(errno));
  }

  LOG(INFO) << "loading original data!!!" << endl;

  std::list<std::pair<content_t, string>> infos;  // 没有随机访问单节点
  for (size_t i = 0; i < img_dirs.size(); i++) {
    auto &&img_dir = img_dirs[i].get<string>();
    const string ann_dir = ann_dirs.empty() ? "" : ann_dirs[i].get<string>();

    auto _infos = load_dota(img_dir, ann_dir, configs.at("nproc"));
    for (auto &&_info : _infos) {
      infos.push_back(std::pair<content_t, string>{_info, img_dir});
    }
  }

  LOG(INFO) << "start splitting images!!!" << endl;
  auto start_time = std::chrono::system_clock::now();

  size_t prog = 0;
  std::mutex lock;
  auto worker = [&configs, &sizes, &gaps, &save_files, &save_imgs, &prog, &lock,
                 &infos](const std::pair<content_t, string> info) {
    vector<float> padding_value(configs.at("padding_value").size(), 0);
    int i = 0;
    for (auto &value : configs.at("padding_value")) {
      padding_value[i++] = value;
    }
    return single_split(
        info, sizes, gaps, configs.at("img_rate_thr"), configs.at("iof_thr"),
        configs.at("no_padding"), padding_value, save_imgs, save_files,
        configs.at("save_ext"), configs.value("ignore_empty_prob", 0.),
        infos.size(), prog, lock);
  };

  const int nthread = configs.at("nproc");
  vector<size_t> patch_infos;
  patch_infos.reserve(infos.size());
  if (nthread > 1) {
    std::threadpool pool(nthread);
    auto _patch_infos = pool.map_container(worker, infos);
    for (auto &_patch_info : _patch_infos) {
      patch_infos.push_back(_patch_info.get());
    }
  } else {
    for (auto &info : infos) {
      patch_infos.push_back(worker(info));
    }
  }

  auto end_time = std::chrono::system_clock::now();
  LOG(INFO) << "finish splitting images in "
            << std::chrono::duration_cast<std::chrono::seconds>(end_time -
                                                                start_time)
                   .count()
            << "s!!!" << endl;

  LOG(INFO) << "splitting images "
            << std::accumulate(patch_infos.begin(), patch_infos.end(), 0UL)
            << " in total" << endl;
}

int main(int argc, char **argv) {
  loguru::init(argc, argv);
  json configs = parse_json(argc, argv);
  GDALAllRegister();
  LOG(INFO) << "\n" << configs.dump(2) << endl;
  deal(configs);
  return 0;
}