#ifndef COMMON_FILE_FILEUTILS_H_
#define COMMON_FILE_FILEUTILS_H_

#include <cstdint>
#include <string>
#include <vector>


namespace file {

bool Exists(const std::string &filename);

bool WriteBytes(const std::string &filename, const uint8_t *contents,
                uint64_t length);

bool ReadBytes(const std::string &filename, std::vector<char> *data);

}  // namespace file

#endif  // COMMON_FILE_FILEUTILS_H_
