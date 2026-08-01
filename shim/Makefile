CXX      = x86_64-w64-mingw32-g++
CXXFLAGS = -O2 -Wall -Wextra -Wno-cast-function-type
LDFLAGS  = -shared

all: dinput8.dll dinput8

dinput8.dll: dinput8_cpp.cpp dinput8.def
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ dinput8_cpp.cpp dinput8.def

clean:
	rm -f dinput8.dll
