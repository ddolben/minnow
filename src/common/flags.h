#ifndef DGB_FLAGS_H_
#define DGB_FLAGS_H_

#include <string>

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

template <typename T>
void RegisterFlag(Flag<T> *flag, std::string name);

template <typename T>
Flag<T>::Flag(std::string name, T default_value)
    : name_(name), value_(default_value) {
  RegisterFlag(this, name);
}

// NOTE: for now, flags must come AFTER standard arguments, because they are
// not currently removed from the argv array.
// TODO: remove flags from argv
void ParseFlags(int *argc, char **argv[]);

#endif  // DGB_FLAGS_H_
