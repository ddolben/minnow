#ifndef MINNOW_GLOBAL_STATS_H_
#define MINNOW_GLOBAL_STATS_H_

#include <cstdint>

namespace dgb {

// Stats contains an arbitrary collection of statistics, currently used only
// for debugging.
struct Stats {
  uint64_t cycle_count = 0;
};

// GlobalStats returns a global instance of Stats.
Stats *GlobalStats();

}  // namespace dgb

#endif  // MINNOW_GLOBAL_STATS_H_
