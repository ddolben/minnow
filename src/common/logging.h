#ifndef DGB_LOGGING_H_
#define DGB_LOGGING_H_

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

// Macros for printing out __LINE__ as a string.
#define TOSTRING(t) #t
#define TOSTRING2(t) TOSTRING(t)
#define LOCATION __FILE__ ":" TOSTRING2(__LINE__)

#define CHECK(e) \
  if (!(e)) { fprintf(stderr, LOCATION ": CHECK failed: %s\n", #e); exit(1); }

#define INFOF(fmt, ...) \
  do { printf("[INFO]: " LOCATION ": " fmt "\n", ##__VA_ARGS__); } while(0)

#define WARNINGF(fmt, ...) \
  do { fprintf(stderr, "[WARNING]: " LOCATION ": " fmt "\n", ##__VA_ARGS__); } while(0)

#define ERRORF(fmt, ...) \
  do { fprintf(stderr, "[ERROR]: " LOCATION ": " fmt "\n", ##__VA_ARGS__); } while(0)

#define FATALF(fmt, ...) \
  do { fprintf(stderr, "[FATAL]: " LOCATION ": " fmt "\n", ##__VA_ARGS__); exit(1); } while(0)

#define DEBUG true
#define DEBUGF(fmt, ...) \
  if (DEBUG) { printf("[DEBUG]: " LOCATION ": " fmt "\n" , ##__VA_ARGS__); }

inline std::string ByteBits(uint8_t byte) {
  char buf[11];
  buf[0] = '0';
  buf[1] = 'b';
  buf[10] = '\0';
  for (int shift = 7; shift >= 0; --shift) {
    buf[9 - shift] = (((byte >> shift) & 0x1) > 0) ? '1' : '0';
  }
  return std::string(buf);
}

#endif  // DGB_LOGGING_H_
