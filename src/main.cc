#include <fcntl.h>
#include <gdal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <atomic>
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
  auto &&gaps = data.at("gaps");
  auto &&sizes = data.at("sizes");
  if (gaps.size() != sizes.size()) {
    LOG(ERROR) << "the sizes of gaps and sizes are not same" << endl;
    exit(1);
  }
  auto &&img_dirs = data.at("img_dirs");
  auto &&ann_dirs = data.at("ann_dirs");
  if (img_dirs.size() != ann_dirs.size()) {
    LOG(ERROR) << "the sizes of img_dirs and ann_dirs are not same" << endl;
    exit(1);
  }

  return data;
}

void deal(const json &configs) {
  vector<int> sizes(configs.at("sizes").begin(), configs.at("sizes").end());
  vector<int> gaps(configs.at("gaps").begin(), configs.at("gaps").end());
  auto &&rates = configs.at("rates");
  for (int i = 0; i < static_cast<int>(rates.size()); i++) {
    sizes[i] /= rates[i].get<float>();
    gaps[i] /= rates[i].get<float>();
  }
  string save_dir = configs.at("save_dir");
  int ret = mkdir(save_dir.c_str(), 0774);
  if (ret == -1) {
    LOG(ERROR) << "mkdir " << save_dir << " " << strerror(errno) << endl;
    exit(1);
  }
  auto &&save_imgs = save_dir + "images";
  auto &&save_files = save_dir + "annfiles";
  ret = mkdir(save_imgs.c_str(), 0774);
  if (ret == -1) {
    LOG(ERROR) << "mkdir " << save_imgs << " " << strerror(errno) << endl;
    exit(1);
  }
  ret = mkdir(save_files.c_str(), 0774);
  if (ret == -1) {
    LOG(ERROR) << "mkdir " << save_files << " " << strerror(errno) << endl;
    exit(1);
  }
  LOG(INFO) << "Loading original data!!!" << endl;
  auto &&img_dirs = configs.at("img_dirs");
  auto &&ann_dirs = configs.at("ann_dirs");
  std::list<std::pair<content_t, string>> infos;  // 没有随机访问单节点
  for (size_t i = 0; i < img_dirs.size(); i++) {
    auto &&img_dir = img_dirs[i].get<string>();
    auto &&ann_dir = ann_dirs[i].get<string>();
    auto _infos = load_dota(img_dir, ann_dir, configs.at("nproc"));
    for (auto &&_info : _infos) {
      infos.push_back(std::pair<content_t, string>{_info, img_dir});
    }
  }

  LOG(INFO) << "start splitting images!!!" << endl;
  auto start_time = std::chrono::system_clock::now();
  auto prog = std::atomic<int>(0);
  auto worker = [configs](const std::pair<content_t, string> info) {
    // return single_split(info, )
  };
  // vector<string> img_dirs(ann_dirs.size());
  // for (int i = 0; i < img_dirs.size(); i++) {
  // }
  auto end_time = std::chrono::system_clock::now();
  LOG(INFO) << "finish splitting images in "
            << std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                     start_time)
                   .count()
            << "s!!!" << endl;
}

int main(int argc, char **argv) {
  loguru::init(argc, argv);
  json configs = parse_json(argc, argv);
  GDALAllRegister();
  LOG(INFO) << "\n" << configs.dump(2) << endl;
  // deal(configs);
  string path =
      "/home/r/Scripts/2023-tianzhibei-competition/mmrotate/tools/tzb-tools/"
      "dota_split/dota_img_split/include/string_utils.hpp";
  auto basename = path::basename(path);
  auto dirname = path::dirname(path);
  auto suffix = path::suffix(path);
  auto stem = path::stem(path);
  LOG(INFO) << basename << endl;
  LOG(INFO) << dirname << endl;
  LOG(INFO) << suffix << endl;
  LOG(INFO) << stem << endl;
  // int ret = path::is_dir(path);
  // if (ret == 0) {
  //   LOG(INFO) << "yes" << endl;
  // } else {
  //   LOG(INFO) << "no" << endl;
  // }
  return 0;
}