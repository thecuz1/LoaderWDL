# Compiler and flags
AR        = x86_64-w64-mingw32-ar
CXX       = x86_64-w64-mingw32-g++
CC        = x86_64-w64-mingw32-gcc
CXXFLAGS += -Wall -Wextra -std=c++20 -ffunction-sections -fdata-sections
CCFLAGS  += -Wall -Wextra -std=c20 -ffunction-sections -fdata-sections

INCLUDES += -I. -Ilua -Iimgui -Iimgui/backends -IUI

LDFLAGS  += -shared -s -Wl,--gc-sections,--exclude-all-symbols
LDLIBS   += -ld3d12 -ld3dcompiler -ldxgi -ldwmapi -lgdi32

ifneq ("$(OFFSETS_156)", "")
    CXXFLAGS += -DOFFSETS_156
endif

# Source files
SRCS      = dllmain.cpp \
            Main.cpp \
            $(wildcard UI/*.cpp) \
            Logger.cpp \

SHIM_SRCS = shim/dinput8.cpp \
			shim/dinput8.def

LIB_SRCS  = MinHook/hook.c \
            MinHook/buffer.c \
            MinHook/trampoline.c \
            MinHook/hde/hde64.c \
            $(filter-out lua/lua.c lua/luac.c, $(wildcard lua/*.c)) \
            $(wildcard imgui/*.cpp) \
            $(wildcard imgui/backends/*.cpp)

# Target
BUILD_DIR  = build
TARGET     = $(BUILD_DIR)/scripthook.dll
SHIM       = $(BUILD_DIR)/dinput8.dll
LIB_TARGET = $(BUILD_DIR)/libdependencies.a

# Map source files to object files inside the build directory
OBJS      = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(SRCS))) \
            $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SRCS))) \

SHIM_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(SHIM_SRCS))) \
            $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SHIM_SRCS)))

LIB_OBJS  = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(LIB_SRCS))) \
            $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(LIB_SRCS)))

# Special flags
$(LIB_OBJS):  CXXFLAGS := -std=c++17 -Os -ffunction-sections -fdata-sections
$(LIB_OBJS):  CCFLAGS  := -std=c17 -Os -ffunction-sections -fdata-sections
$(SHIM):      LDFLAGS  := -shared -s -Wl,--gc-sections

# Default rule
all: $(TARGET)

-include Makefile-local.mk

# Linking
$(TARGET): $(LIB_TARGET) $(OBJS) $(SHIM)
	$(CXX) $(OBJS) $(LIB_TARGET) $(LDFLAGS) $(LDLIBS) -o $@

$(SHIM): $(SHIM_OBJS)
	$(CXX) $(SHIM_OBJS) $(LDFLAGS) -o $@

# .cpp source files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# .c source files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCLUDES) -c $< -o $@

# Build libraries into a static archive (.a) this reduces size for target
$(LIB_TARGET): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

clean:
	rm -rf $(BUILD_DIR)

PHONY_TARGETS += all clean

.PHONY: $(PHONY_TARGETS)
