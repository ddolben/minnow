#ifndef DGB_FLAGS_H_
#define DGB_FLAGS_H_

#include <map>
#include <string>


// Class wrapping a runtime flag variable.
template <typename T>
class Flag {
 public:
  Flag(std::string name, T default_value);
  void set_value(T value) { value_ = value; }
  T operator*() { return value_; }
  const std::string &name() { return name_; }

 private:
  std::string name_;
  T value_;
};

// Class used for registering flags at init time. Doing this in the header with
// static methods and a static instance allows it to work regardless of when
// flags.o is loaded.
template <typename T>
class FlagRegistrar {
 public:
  static FlagRegistrar *Instance() {
    static FlagRegistrar *instance = nullptr;
    if (instance == nullptr) {
      instance = new FlagRegistrar();
    }
    return instance;
  }

  static bool HasFlag(const std::string &name) {
    return Instance()->flags_.find(name) != Instance()->flags_.end();
  }

  static void SetValue(const std::string &name, T value) {
    Instance()->flags_[name]->set_value(value);
  }

  void RegisterFlag(Flag<T> *flag, const std::string &name) {
    flags_[name] = flag;
  }

 private:
  std::map<std::string, Flag<T>*> flags_;
};

template <typename T>
Flag<T>::Flag(std::string name, T default_value)
    : name_(name), value_(default_value) {
  FlagRegistrar<T>::Instance()->RegisterFlag(this, name);
}

// Parse flags out from argv.
// NOTE: for now, flags must come AFTER standard arguments, because they are
// not currently removed from the argv array.
// TODO: remove flags from argv
void ParseFlags(int *argc, char **argv[]);

#endif  // DGB_FLAGS_H_
