#include "split_utils.h"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <string>
#include <vector>

using std::endl;
using std::string;
using std::vector;

void single_split(const std::pair<content_t, string>& arguments,
                  const vector<int>& sizes, const vector<int>& gaps,
                  const float& img_rate_thr, const float& iof_thr,
                  const bool& no_padding, const vector<float>& padding_value,
                  const string& save_dir, const string& anno_dir,
                  const string& img_ext, const std::atomic<int>& prog) {}
