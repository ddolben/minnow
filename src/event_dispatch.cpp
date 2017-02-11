#include "event_dispatch.h"

namespace dgb {

static std::shared_ptr<EventDispatch> g_dispatch(new EventDispatch());

std::shared_ptr<EventDispatch> GlobalDispatch() {
  return g_dispatch;
}

}  // namespace dgb
