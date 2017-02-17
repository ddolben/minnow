#include "common/file/fileutils.h"

#include <fstream>
#include <string>

#include "common/logging.h"

namespace file {

bool Exists(const std::string &filename) {
  std::ifstream f(filename);
  return f.good();
}

bool WriteBytes(const std::string &filename, const uint8_t *contents,
                uint64_t length) {
  CHECK(contents != nullptr);

  // TODO: errors
  std::ofstream fout;
  fout.open(filename, std::ios::binary | std::ios::out);
  fout.write(reinterpret_cast<const char*>(contents), length);
  fout.close();
  return true;
}

bool ReadBytes(const std::string &filename, std::vector<char> *data) {
  CHECK(data != nullptr);

  // TODO; errors
  std::ifstream fin(filename);
  if (!fin.good()) return false;
  std::vector<char> contents((std::istreambuf_iterator<char>(fin)), 
      std::istreambuf_iterator<char>());
  data->swap(contents);
  fin.close();
  return true;
}

}  // namespace file
