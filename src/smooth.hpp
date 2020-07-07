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

namespace smoothxg {

using namespace handlegraph;

void smooth(const xg::XG& graph,
            const block_t& block);

void write_gfa(std::unique_ptr<spoa::Graph>& graph,
               std::ostream& out,
               const std::vector<std::string>& sequence_names,
               bool include_consensus);

}