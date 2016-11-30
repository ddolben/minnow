CXX := clang++

SRC_DIR := src
OBJ_DIR := obj
EXE := minnow

LIB_INCLUDES := -I/usr/include -I/usr/local/include
LIBS := -lSDL2
CXX_FLAGS := -Wall --std=c++0x -g -I$(SRC_DIR) $(LIB_INCLUDES)
LD_FLAGS := $(LIBS)

# Generate lists of srcs and object code, including proto generated code.
# NOTE: sort removes duplicates
HDRS := $(sort $(shell find $(SRC_DIR) -type f -name '*.h'))
INLINES := $(sort $(shell find $(SRC_DIR) -type f -name '*.inl'))
SRCS := $(sort $(shell find $(SRC_DIR) -type f -name '*.cpp'))
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

all: $(EXE)

# Build the executable.
$(EXE): $(OBJS) $(HDRS) $(INLINES)
	$(CXX) $(CXX_FLAGS) $(LD_FLAGS) -o $@ $(OBJS)

# Build all of the source files into object files.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS) $(INLINES)
	@# Quietly make sure the directory exists.
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(PROTO_SRCS) $(PROTO_HDRS) $(EXE)
