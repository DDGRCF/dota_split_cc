#ifndef SPLIT_UTILS_H_
#define SPLIT_UTILS_H_

#include <mutex>
#include <string>
#include <vector>

#include "dota_utils.h"

size_t single_split(const std::pair<content_t, std::string>& arguments,
                    const std::vector<int>& sizes, const std::vector<int>& gaps,
                    const float& img_rate_thr, const float& iof_thr,
                    const bool& no_padding,
                    const std::vector<float>& padding_value,
                    const std::string& save_dir, const std::string& anno_dir,
                    const std::string& img_ext, const float& ignore_empty_prob,
                    const size_t& total, size_t& prog, std::mutex& lock);

#endif