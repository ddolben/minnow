#ifndef MINNOW_EVENT_DISPATCH_H_
#define MINNOW_EVENT_DISPATCH_H_

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace dgb {

enum EventCode {
  EVENT_UNKNOWN = 0,
  EVENT_START_DEBUGGER,
  EVENT_TOGGLE_PAUSE
};

class Event {
 public:
  Event(EventCode code) : code_(code) {}
  EventCode code() const { return code_; }

 private:
  EventCode code_ = EVENT_UNKNOWN;
};

typedef std::function<void(const Event &event)> Observer;

class EventDispatch {
 public:
  void FireEvent(const Event &event) {
    if (observers_.count(event.code()) == 0) return;
    for (Observer &observer : observers_[event.code()]) {
      observer(event);
    }
  }

  void RegisterObserver(EventCode code, Observer observer) {
    observers_[code].push_back(observer);
  }

 private:
  // TODO: mutex
  std::map<EventCode, std::vector<Observer>> observers_;
};

// Returns a globally shared event dispatch instance. Normally, you would want
// to create a shared pointer in main() and pass it into your classes, but this
// can be useful for quickly triggering the debugger from a class that doesn't
// have a reference to it.
std::shared_ptr<EventDispatch> GlobalDispatch();

}  // namespace dgb

#endif  // MINNOW_EVENT_DISPATCH_H_
