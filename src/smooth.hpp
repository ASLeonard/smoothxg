#pragma once

#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include "spoa/spoa.hpp"
#include "xg.hpp"
#include "blocks.hpp"
#include "odgi/odgi.hpp"
#include "odgi/unchop.hpp"
#include "odgi/topological_sort.hpp"

namespace smoothxg {

struct path_position_range_t {
    path_handle_t base_path;
    uint64_t start_pos;
    uint64_t end_pos;
    path_handle_t target_path;
    uint64_t target_graph_id;
};

odgi::graph_t smooth(const xg::XG& graph,
                     const block_t& block,
                     const std::string& consensus_name = "");

odgi::graph_t smooth_and_lace(const xg::XG& graph,
                              const std::vector<block_t>& blocks,
                              const std::string& consensus_name = "");

void write_gfa(std::unique_ptr<spoa::Graph>& graph,
               std::ostream& out,
               const std::vector<std::string>& sequence_names,
               bool include_consensus);

void build_odgi(std::unique_ptr<spoa::Graph>& graph,
                odgi::graph_t& output,
                const std::vector<std::string>& sequence_names,
                const std::string& consensus_name,
                bool include_consensus = true);


}
