# Compiler and flags
CXX       = x86_64-w64-mingw32-g++
CC        = x86_64-w64-mingw32-gcc
CXXFLAGS += -Wall -std=c++17 -ffunction-sections -fdata-sections
CCFLAGS  += -Wall -std=c17 -ffunction-sections -fdata-sections

INCLUDES += -I. -Ilua -Iimgui -Iimgui/backends -IUI

LDFLAGS  += -shared -s -Wl,--gc-sections,--exclude-all-symbols
LDLIBS   += -ld3d12 -ld3dcompiler -ldxgi -ldwmapi -lgdi32

ifneq ("$(OFFSETS_156)", "")
    CXXFLAGS += -DOFFSETS_156
endif

# Source files
SRCS     = dllmain.cpp \
           Main.cpp \
           $(wildcard UI/*.cpp) \
           Logger.cpp \
           MinHook/hook.c \
           MinHook/buffer.c \
           MinHook/trampoline.c \
           MinHook/hde/hde64.c \
           $(filter-out lua/lua.c lua/luac.c, $(wildcard lua/*.c)) \
           $(wildcard imgui/*.cpp) \
           $(wildcard imgui/backends/*.cpp)

# Target
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/scripthook.dll

# Map source files to object files inside the build directory
OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(SRCS))) \
       $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SRCS)))

# Default rule
all: $(TARGET)

-include Makefile-local.mk

# Linking
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# .cpp source files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# .c source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

PHONY_TARGETS += all clean

.PHONY: $(PHONY_TARGETS)
