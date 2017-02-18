#ifndef COMMON_TEST_TEST_H_
#define COMMON_TEST_TEST_H_

#include <functional>
#include <iostream>
#include <string>
#include <vector>

class TestWrapper {
 public:
  TestWrapper(const std::string &name, std::function<int()> f)
    : name_(name), f_(f) {}

  const std::string &name() const { return name_; }

  int Execute() const {
    if (f_ == nullptr) {
      printf("Test '%s' missing a test body.\n", name_.c_str());
      return 0;
    }
    return f_();
  }

 private:
  std::string name_;
  std::function<int()> f_;
};

class TestRegistry {
 public:
  static TestRegistry *Instance() {
    static TestRegistry *registry = nullptr;
    if (registry == nullptr) {
      registry = new TestRegistry();
    }
    return registry;
  }

  void RegisterTest(TestWrapper t) {
    tests_.push_back(t);
  }

  const std::vector<TestWrapper> &tests() { return tests_; }

 private:
  std::vector<TestWrapper> tests_;
};

// Class used to register tests at init time via the constructor.
class RegisterTest {
 public:
  RegisterTest(std::string name, std::function<int()> t) {
    printf("Registering test '%s'.\n", name.c_str());
    TestRegistry::Instance()->RegisterTest(TestWrapper(name, t));
  }
};

#define ASSERT_TRUE(exp) \
  if (!(exp)) { \
    fprintf(stderr, "ASSERT_TRUE failed: " #exp); \
    return 1; \
  }

#define ASSERT_EQ(a, b) \
  if ((a) != (b)) { \
    std::cerr << "  " << __FILE__ << ":" << __LINE__ \
        << ": ASSERT_EQ failed: " #a " != " #b << std::endl \
        << "    " << (a) << " != " << (b) << std::endl; \
    return 1; \
  }

#define TEST(name) \
  int TEST_##name(); \
  RegisterTest REGISTER_TEST_##name(#name, TEST_##name); \
  int TEST_##name()

#endif  // COMMON_TEST_TEST_H_
