#ifndef COMMON_STRING_STRING_H_
#define COMMON_STRING_STRING_H_

#include <iostream>
#include <sstream>
#include <string>
#include <vector>


std::vector<std::string> StringSplit(std::string str, char delimiter) {
  std::vector<std::string> splits;
  std::istringstream tokens(str);
  std::string s;
  while (std::getline(tokens, s, delimiter)) {
    std::cout << s << std::endl;
    splits.push_back(s);
  }
  return splits;
}

#endif  // COMMON_STRING_STRING_H_
