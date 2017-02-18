#include "flags.h"

#include <cstring>
#include <string>
#include <map>
#include <vector>

#include "logging.h"

namespace {

const static std::string kTrueString{"true"};

bool ParseBoolFromFlag(const std::vector<std::string> splits) {
  // No split means it's a standalone flag, which means true.
  if (splits.size() < 2) return true;

  return kTrueString.compare(splits[1]) == 0;
}

}  // namespace

void ParseFlags(int *argc, char **argv[]) {
  INFOF("Flags:");

  for (int i = 1; i < *argc; ++i) {
    const char *str = (*argv)[i];
    if (strlen(str) < 3) continue;
    if (str[0] != '-' || str[1] != '-') continue;
    const char *raw_flag = str + 2;

    std::vector<std::string> splits;

    const char *split = strchr(raw_flag, '=');
    if (split == nullptr) {
      splits.push_back(raw_flag);
    } else {
      splits.push_back(
          std::string(raw_flag, 0, static_cast<int>(split - raw_flag)));
      splits.push_back(std::string(split + 1));
    }

    const std::string &name = splits[0];

    // Do bool flags first so we don't fail if there's no '=' character.
    if (FlagRegistrar<bool>::HasFlag(name)) {
      bool value = ParseBoolFromFlag(splits);
      FlagRegistrar<bool>::SetValue(name, value);
      INFOF("  %s = %s", name.c_str(), (value) ? "true" : "false");
      continue;
    }

    if (splits.size() < 2) {
      FATALF("Flag missing value: %s", str);
    }
    const std::string &value = splits[1];

    if (FlagRegistrar<std::string>::HasFlag(name)) {
      FlagRegistrar<std::string>::SetValue(name, value);
      INFOF("  %s = %s", name.c_str(), value.c_str());
      continue;
    }
    FATALF("Unrecognized flag: %s = %s", name.c_str(), value.c_str());
  }
}
