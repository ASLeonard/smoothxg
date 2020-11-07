#include "breaks.hpp"
#include "progress.hpp"

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
                  const bool& consensus_graph) {

    const VectorizableHandleGraph& vec_graph = dynamic_cast<const VectorizableHandleGraph&>(graph);

    std::stringstream splits_banner;
    splits_banner << "[smoothxg::break_blocks] splitting short sequences out of " << blocks.size() << " blocks:";
    progress_meter::ProgressMeter splits_progress(blocks.size(), splits_banner.str());

    std::atomic<uint64_t> split_blocks;
    split_blocks.store(0);
    std::mutex new_blocks_mutex;
    std::vector<std::pair<double, block_t>> new_blocks;
    paryfor::parallel_for<uint64_t>(
        0, blocks.size(), thread_count,
        [&](uint64_t block_id, int tid) {
            //for (auto& block : blocks) {
            auto &block = blocks[block_id];
            // ensure that the sequences in the block
            // are within our identity threshold
            // if not, peel them off into splits
            //EdlibAlignResult result = edlibAlign("hello", 5, "world!", 6, edlibDefaultAlignConfig());
            std::vector<std::string> seqs;
            for (auto& path_range : block.path_ranges) {
                std::string name = graph.get_path_name(graph.get_path_handle_of_step(path_range.begin));
                seqs.emplace_back();
                auto& seq = seqs.back();
                for (step_handle_t step = path_range.begin;
                     step != path_range.end;
                     step = graph.get_next_step(step)) {
                    seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
                }
            }
            std::vector<std::vector<uint64_t>> groups;
            // iterate through the seqs
            // for each sequence try to match it to a group at the given identity threshold
            // if we can't get it to match, add a new group
            groups.emplace_back();
            groups.back().push_back(0); // seed with the first sequence
            for (uint64_t i = 1; i < seqs.size(); ++i) {
                auto& curr = seqs[i];
                uint64_t best_group = 0;
                double best_id = -1;
                for (uint64_t j = 0; j < groups.size(); ++j) {
                    auto& group = groups[j];
                    for (uint64_t k = 0; k < group.size(); ++k) {
                        auto& other = seqs[k];
                        EdlibAlignResult result = edlibAlign(curr.c_str(), curr.size(), other.c_str(), other.size(),
                                                             edlibNewAlignConfig(-1, EDLIB_MODE_HW, EDLIB_TASK_DISTANCE, NULL, 0));
                        if (result.status == EDLIB_STATUS_OK) {
                            double id = (double)(curr.size() - result.editDistance) / (double)curr.size();
                            if (id > best_id) {
                                best_group = j;
                                best_id = id;
                            }
                        }
                    }
                }
                if (best_id > 0) {
                    groups[best_group].push_back(i);
                } else {
                    groups.push_back({i});
                }
            }
            if (groups.size() == 1) {
                // nothing to do
                {
                    std::lock_guard<std::mutex> guard(new_blocks_mutex);
                    new_blocks.push_back(std::make_pair(block_id, block));
                }
            } else {
                ++split_blocks;
                uint64_t i = 0;
                for (auto& group : groups) {
                    block_t new_block;
                    new_block.is_split = true;
                    for (auto& i : group) {
                        new_block.path_ranges.push_back(block.path_ranges[i]);
                    }
                    for (auto& path_range : new_block.path_ranges) {
                        new_block.total_path_length += path_range.length;
                        new_block.max_path_length = std::max(new_block.max_path_length,
                                                             path_range.length);
                    }
                    {
                        std::lock_guard<std::mutex> guard(new_blocks_mutex);
                        new_blocks.push_back(std::make_pair(block_id + i++ * (1.0/groups.size()), new_block));
                    }
                }
            }
            splits_progress.increment(1);
        });
    splits_progress.finish();
    std::vector<block_t>().swap(blocks); // clear blocks
    ips4o::parallel::sort(
        new_blocks.begin(), new_blocks.end(),
        [](const std::pair<double, block_t>& a,
           const std::pair<double, block_t>& b) {
            return a.first < b.first;
        });
    for (auto& p : new_blocks) {
        blocks.push_back(p.second);
    }
    std::vector<std::pair<double, block_t>>().swap(new_blocks); // clear new_blocks

    std::cerr << "[smoothxg::break_blocks] split " << split_blocks << " blocks" << std::endl;
    std::cerr << "[smoothxg::break_blocks] cutting blocks that contain sequences longer than max-poa-length (" << max_poa_length << ")" << std::endl;
    
    std::stringstream breaks_banner;
    breaks_banner << "[smoothxg::break_blocks] cutting " << blocks.size() << " blocks:";
    progress_meter::ProgressMeter breaks_progress(blocks.size(), breaks_banner.str());
    
    uint64_t n_cut_blocks = 0;
    uint64_t n_repeat_blocks = 0;
    paryfor::parallel_for<uint64_t>(
        0, blocks.size(), thread_count,
        [&](uint64_t block_id, int tid) {
            auto &block = blocks[block_id];
            // check if we have sequences that are too long
            bool to_break = false;
            for (auto& path_range : block.path_ranges) {
                if (path_range.length > max_poa_length) {
                    to_break = true;
                    break;
                }
            }
            if (block.path_ranges.size() > 1 && to_break) {
                ++n_cut_blocks;
                uint64_t cut_length = max_poa_length;
                bool found_repeat = false;
                // otherwise let's see if we've got repeats that we can use to chop things up
                // find if there is a repeat
                if (break_repeats) {
                    std::vector<sautocorr::repeat_t> repeats;
                    for (auto& path_range : block.path_ranges) {
                        // steps in id space
                        std::string seq;
                        std::string name = graph.get_path_name(graph.get_path_handle_of_step(path_range.begin));
                        for (step_handle_t step = path_range.begin;
                             step != path_range.end;
                             step = graph.get_next_step(step)) {
                            seq.append(graph.get_sequence(graph.get_handle_of_step(step)));
                        }
                        if (seq.length() < 2*min_copy_length) continue;
                        //std::cerr << "on " << name << "\t" << seq.length() << std::endl;
                        std::vector<uint8_t> vec(seq.begin(), seq.end());
                        sautocorr::repeat_t result = sautocorr::repeat(vec,
                                                                       min_copy_length,
                                                                       max_copy_length,
                                                                       min_copy_length,
                                                                       min_autocorr_z,
                                                                       autocorr_stride);
                        repeats.push_back(result);
                    }
                    // if there is, set the cut length to some fraction of it
                    std::vector<double> lengths;
                    double max_z = 0;
                    for (auto& repeat : repeats) {
                        if (repeat.length > 0) {
                            lengths.push_back(repeat.length);
                            max_z = std::max(repeat.z_score, max_z);
                        }
                    }
                    found_repeat = !lengths.empty();
                    if (found_repeat) {
                        double repeat_length = sautocorr::vec_mean(lengths.begin(), lengths.end());
                        cut_length = std::round(repeat_length / 2.0);
                        ++n_repeat_blocks;
                        //std::cerr << "found repeat of " << repeat_length << " and Z-score " << max_z << " cutting to " << cut_length << std::endl;
                    } else {
                        // if not, chop blindly
                        cut_length = max_poa_length;
                    }
                }
                std::vector<path_range_t> chopped_ranges;
                for (auto& path_range : block.path_ranges) {

                    if (!found_repeat && path_range.length < cut_length) {
                        chopped_ranges.push_back(path_range);
                        continue;
                    }
                    // now find outlier clusters based on stdev and mean
                    // extract a minimum viable repeat length
                    // scan across the step vector, looking for where the repeat region begins and ends
                    // cut at the repeat boundaries

                    // Q: should we determine the repeat length for each sequence or all?
                    // each is simple, but maybe expensive
                    // all could provide higher precision, but it's muddier

                    // if this doesn't work, we're going to blindly cut anyway
                    uint64_t last_cut = 0;
                    step_handle_t last_end = path_range.begin;
                    //path_range_t* new_range = nullptr;
                    uint64_t pos = 0;
                    step_handle_t step;
                    for (step = path_range.begin;
                         step != path_range.end;
                         step = graph.get_next_step(step)) {
                        //handle_t h = graph.get_handle_of_step(step);
                        //uint64_t id = graph.get_id(h);
                        //int64_t node_pos = vec_graph.node_vector_offset(id);
                        pos += graph.get_length(graph.get_handle_of_step(step));
                        if (pos - last_cut > cut_length) {
                            step_handle_t next = graph.get_next_step(step);
                            chopped_ranges.push_back({last_end, next, pos - last_cut});
                            last_end = next;
                            last_cut = pos;
                        }
                    }
                    if (step != last_end) {
                        chopped_ranges.push_back({last_end, step, pos - last_cut});
                    }
                }
                block.path_ranges = chopped_ranges;
                // order the path ranges from longest/shortest to shortest/longest
                ips4o::parallel::sort(
                    block.path_ranges.begin(), block.path_ranges.end(),
                    order_paths_from_longest
                    ?
                    [](const path_range_t& a,
                       const path_range_t& b) {
                        return a.length > b.length;
                    }
                    :
                    [](const path_range_t& a,
                       const path_range_t& b) {
                        return a.length < b.length;
                    }
                    );
                block.broken = true;
                block.is_repeat = found_repeat;
                breaks_progress.increment(1);
            }
            // prepare the path_ranges_t for a consensus graph if necessary
            // we do this here, because it works in parallel
            if (consensus_graph) {
                for (auto& path_range : block.path_ranges) {
                    path_range.nuc_begin = graph.get_position_of_step(path_range.begin);
                    path_range.nuc_end = graph.get_position_of_step(path_range.end);
                }
            }
        });
    breaks_progress.finish();
    std::cerr << "[smoothxg::break_blocks] cut " << n_cut_blocks << " blocks of which " << n_repeat_blocks << " had repeats" << std::endl;
}

}
