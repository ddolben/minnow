#include "flags.h"

#include <cstring>
#include <string>
#include <map>

#include "logging.h"

namespace {

std::map<std::string, Flag<std::string>*> string_flags;

}  // namespace

template <>
void RegisterFlag(Flag<std::string> *flag, std::string name) {
  string_flags[name] = flag;
}

void ParseFlags(int *argc, char **argv[]) {
  INFOF("Flags:");

  for (int i = 1; i < *argc; ++i) {
    const char *str = (*argv)[i];
    if (strlen(str) < 3) continue;
    if (str[0] != '-' || str[1] != '-') continue;
    const char *raw_flag = str + 2;
    const char *split = strchr(raw_flag, '=');
    if (split == nullptr) {
      FATALF("Flag missing value: '%s'", raw_flag);
    }
    std::string name(raw_flag, 0, static_cast<int>(split - raw_flag));
    std::string value(split + 1);

    if (string_flags.find(name) != string_flags.end()) {
      string_flags[name]->set_value(value);
      INFOF("  %s = %s", name.c_str(), value.c_str());
      continue;
    }
    FATALF("Unrecognized flag: %s = %s", name.c_str(), value.c_str());
  }
}
