# ---------------------------------------------------------------------------
#  Echoes of the Sunken Spire - native C++ port
#
#    make            build ./spire  (needs an assets dir, see README)
#    make run        build and launch (assets/ next to the binary)
#    make run-game   build and launch (assets/ must sit next to the binary)
#    make clean
#
#  Linux:   needs g++ and the X11 headers (libx11-dev)
#  Windows: use build.bat (MSVC) - the same sources compile there
# ---------------------------------------------------------------------------
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wno-unused-function -Wno-unused-variable
LDFLAGS  ?=
LDLIBS   ?= -lX11 -pthread
SRC      := $(filter-out src/assets_index.cpp,$(wildcard src/*.cpp))
OBJ      := $(patsubst src/%.cpp,build/%.o,$(SRC))
BIN      := spire

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

# -MMD -MP writes build/foo.d next to each object. Without this a change to
# base.h/render.h leaves stale objects compiled against the old struct layout,
# which links happily and then corrupts the heap at exit.
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(OBJ:.o=.d)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build $(BIN)

.PHONY: all clean run
