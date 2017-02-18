#include "common/test/test.h"

#include <cstdio>
#include <functional>
#include <vector>


int main(int argc, const char *argv[]) {
  int total_tests = TestRegistry::Instance()->tests().size();
  int failed_tests = 0;

  printf("\n");

  for (const TestWrapper &t : TestRegistry::Instance()->tests()) {
    printf("Running test: %s\n", t.name().c_str());

    int result = t.Execute();
    if (result != 0) {
      printf("  \033[0;31mFAIL\033[0m\n");
    } else {
      printf("  \033[0;32mPASS\033[0m\n");
    }

    failed_tests += result;
  }

  printf("\n[ %d / %d ] tests passed\n",
      total_tests - failed_tests, total_tests);

  if (failed_tests == 0) {
    printf("  \033[0;32mALL PASS\033[0m\n");
  } else {
    printf("  \033[0;31mFAIL\033[0m\n");
  }

  return failed_tests;
}
