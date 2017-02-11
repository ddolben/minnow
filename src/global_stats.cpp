#include "global_stats.h"

namespace dgb {

static Stats g_stats = {};

Stats *GlobalStats() {
  return &g_stats;
}

}  // namespace dgb
