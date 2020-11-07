#pragma once

#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <vector>
#include <vector>
#include <numeric>
#include <cmath>
#include "edlib.h"
#include "blocks.hpp"
#include "sautocorr.hpp"
#include "xg.hpp"
#include "paryfor.hpp"

namespace smoothxg {

using namespace handlegraph;

// break the path ranges at likely VNTR boundaries
// and break the path ranges to be shorter than our "max" sequence size input to spoa
void break_blocks(const xg::XG& graph,
                  std::vector<block_t>& blocks,
                  const double& block_group_identity,
                  const uint64_t& max_poa_length,
                  const uint64_t& min_copy_length,
                  const uint64_t& max_copy_length,
                  const uint64_t& min_autocorr_z,
                  const uint64_t& autocorr_stride,
                  const bool& order_paths_from_longest,
                  const bool& break_repeats,
                  const uint64_t& thread_count,
                  const bool& consensus_graph);

}
