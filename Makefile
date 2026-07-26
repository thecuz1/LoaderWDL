# Compiler and flags
CXX      = x86_64-w64-mingw32-g++
CXXFLAGS += -Wall -std=c++17 -fpermissive
LDFLAGS  += -shared -static -s -Wl,--exclude-all-symbols

ifneq ("$(OFFSETS_156)", "")
    CXXFLAGS += -DOFFSETS_156
endif

# Source files
SRCS     = dllmain.cpp \
           Main.cpp \
           Logger.cpp \
           MinHook/hook.c \
           MinHook/buffer.c \
           MinHook/trampoline.c \
           MinHook/hde/hde64.c

# Target
BUILD_DIR = build
TARGET   = $(BUILD_DIR)/scripthook.dll

# Map source files to object files inside the build directory
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(SRCS))) \
       $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SRCS)))

# Default rule
all: $(TARGET)

-include Makefile-local.mk

# Linking
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

# .cpp source files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# .c source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

PHONY_TARGETS += all clean

.PHONY: $(PHONY_TARGETS)
