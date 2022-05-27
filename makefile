CXX := clang++

SRC_DIR := src
OBJ_DIR := obj
EXE := minnow
TEST_EXE := minnow_test

LIB_INCLUDES := -I/usr/include -I/usr/local/include
LIBS := -lSDL2
CXX_FLAGS := -Wall --std=c++0x -g -I$(SRC_DIR) $(LIB_INCLUDES)
LD_FLAGS := $(LIBS)

# Generate lists of srcs and object code.
# NOTE: sort removes duplicates
HDRS := $(sort $(shell find $(SRC_DIR) -type f -name '*.h'))
INLINES := $(sort $(shell find $(SRC_DIR) -type f -name '*.inl'))

SRCS := 		 $(sort $(shell find $(SRC_DIR) -type f -name '*.cpp' ! -name 'main.cpp' ! -name '*test_main.cpp' ! -name '*_test.cpp'))
MAIN_SRCS := $(sort $(shell find $(SRC_DIR) -type f -name 'main.cpp'))
TEST_SRCS := $(sort $(shell find $(SRC_DIR) -type f -name '*_test.cpp' -o -name '*test_main.cpp'))

OBJS :=      $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
MAIN_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(MAIN_SRCS))
TEST_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(TEST_SRCS))

all: $(EXE)

tests: $(TEST_EXE)

.PHONY: test
test: tests
	./minnow_test

# Build the executable.
$(EXE): $(MAIN_OBJS) $(OBJS) $(HDRS) $(INLINES)
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) -o $@ $(MAIN_OBJS) $(OBJS)

$(TEST_EXE): $(TEST_OBJS) $(OBJS) $(HDRS) $(INLINES)
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) -o $@ $(TEST_OBJS) $(OBJS)

# Build all of the source files into object files.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS) $(INLINES)
	@# Quietly make sure the directory exists.
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(EXE) $(TEST_EXE)
