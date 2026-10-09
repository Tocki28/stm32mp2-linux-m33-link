# Build, check and run the link test.
#   make            build both programs for this Mac, into build/
#   make test       build the test program and run it (fake channel, no hardware)
#   make coverage   run the tests and report which flight lines and branches they ran
#   make analyze    static analysis of both programs: clang analyzer + cppcheck
#   make image      create the Docker build image from docker/Dockerfile (once)
#   make board      build the flight program for the STM32MP2 (64-bit ARM Linux), in Docker
#   make deploy     build the flight program for the board and copy it there (/tmp/link_test_arm64)
#   make clean      delete everything built

# Folders, laid out like a module of NASA's core Flight Executive (cFE):
#   config/        every setting, each with its reason
#   fsw/inc/       flight software, public: what code outside this module may include
#   fsw/src/       flight software, private: the code and its own headers
#   ut-coverage/   unit tests of the flight code, plus their helpers; Mac only
#   ut-stubs/      fakes that stand in for the layer below; Mac only
#   tools/         scripts the build runs
#   docker/        the 64-bit ARM Linux build environment (make image)
#   build/         built programs; make clean deletes it, make rebuilds it
CONFIG_DIR = config
INC_DIR    = fsw/inc
SRC_DIR    = fsw/src
UT_DIR     = ut-coverage
STUB_DIR   = ut-stubs
BUILD_DIR  = build

# Two programs from the same flight files:
#   link_test   FLIGHT program: fsw/ and config/ only, real rpmsg channel. Runs on the board.
#   unit_test   TEST program: every flight file except main.cpp, plus ut-coverage/ and
#               ut-stubs/. Runs on this Mac, never on the board.
# SHARED is found automatically, so a new flight file is tested without editing this list.
SHARED = $(filter-out $(SRC_DIR)/main.cpp,$(wildcard $(SRC_DIR)/*.cpp))
FLIGHT = $(SRC_DIR)/main.cpp $(SHARED)
TEST   = $(wildcard $(UT_DIR)/*.cpp) $(wildcard $(STUB_DIR)/*.cpp) $(SHARED)
FLIGHT_HEADERS = $(wildcard $(CONFIG_DIR)/*.hpp $(INC_DIR)/*.hpp $(SRC_DIR)/*.hpp)
TEST_HEADERS   = $(wildcard $(UT_DIR)/*.hpp $(STUB_DIR)/*.hpp)

# Which folders each build may include from. The flight build is not given the test
# folders, so it cannot even find a test-only header by name.
FLIGHT_INCLUDES = -I$(CONFIG_DIR) -I$(INC_DIR) -I$(SRC_DIR)
TEST_INCLUDES   = $(FLIGHT_INCLUDES) -I$(STUB_DIR) -I$(UT_DIR)

# Two more guards keep test code out of the flight program, whatever route it takes
# (for example an include written with a path, like "../../ut-stubs/..."):
#   guard 1  test-only headers refuse to compile unless this is defined, and only the
#            test build defines it (see ut-stubs/loopback_transport.hpp)
#   guard 2  every finished flight program is searched for test code and deleted if
#            any is found (see tools/check_flight_binary.sh)
TEST_BUILD = -DLINK_TEST_BUILD
CHECK_FLIGHT = ./tools/check_flight_binary.sh

# Power of Ten rule 10: every warning on, and warnings are errors.
WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
           -Wold-style-cast -Wcast-align -Wnull-dereference -Wformat=2 -Werror

# No exceptions and no run-time type information: errors travel as Status values.
FLAGS = -std=c++17 -O2 -fno-exceptions -fno-rtti $(WARNINGS)

BOARD = root@192.168.7.1
IMAGE = stm32mp2-arm64-build

all: $(BUILD_DIR)/link_test $(BUILD_DIR)/unit_test

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/link_test: $(FLIGHT) $(FLIGHT_HEADERS) | $(BUILD_DIR)
	clang++ $(FLAGS) $(FLIGHT_INCLUDES) -o $@ $(FLIGHT)
	$(CHECK_FLIGHT) $@

$(BUILD_DIR)/unit_test: $(TEST) $(FLIGHT_HEADERS) $(TEST_HEADERS) | $(BUILD_DIR)
	clang++ $(FLAGS) $(TEST_BUILD) $(TEST_INCLUDES) -o $@ $(TEST)

test: $(BUILD_DIR)/unit_test
	./$(BUILD_DIR)/unit_test

# Coverage, as cFE measures it: build the tests with counters in every line and
# branch, run them, then report how much of each flight file they ran.
coverage: | $(BUILD_DIR)
	clang++ $(FLAGS) $(TEST_BUILD) $(TEST_INCLUDES) -fprofile-instr-generate -fcoverage-mapping \
	  -o $(BUILD_DIR)/unit_test_coverage $(TEST)
	LLVM_PROFILE_FILE=$(BUILD_DIR)/unit_test.profraw ./$(BUILD_DIR)/unit_test_coverage > /dev/null 2>&1
	xcrun llvm-profdata merge -o $(BUILD_DIR)/unit_test.profdata $(BUILD_DIR)/unit_test.profraw
	xcrun llvm-cov report $(BUILD_DIR)/unit_test_coverage \
	  -instr-profile=$(BUILD_DIR)/unit_test.profdata $(SHARED)

analyze:
	clang++ -std=c++17 --analyze -Xanalyzer -analyzer-output=text $(FLIGHT_INCLUDES) $(FLIGHT)
	clang++ -std=c++17 --analyze -Xanalyzer -analyzer-output=text $(TEST_BUILD) $(TEST_INCLUDES) $(TEST)
	cppcheck --enable=warning,style,performance,portability --std=c++17 \
	  --inline-suppr --error-exitcode=1 --quiet $(FLIGHT_INCLUDES) $(FLIGHT)
	cppcheck --enable=warning,style,performance,portability --std=c++17 \
	  --inline-suppr --error-exitcode=1 --quiet $(TEST_BUILD) $(TEST_INCLUDES) $(TEST)
	rm -f *.plist

image:
	docker build --platform linux/arm64 -t $(IMAGE) docker

$(BUILD_DIR)/link_test_arm64: $(FLIGHT) $(FLIGHT_HEADERS) | $(BUILD_DIR)
	docker run --rm --platform linux/arm64 -v "$(CURDIR)":/src $(IMAGE) \
	  g++ $(FLAGS) $(FLIGHT_INCLUDES) -static -o $@ $(FLIGHT)
	$(CHECK_FLIGHT) $@

board: $(BUILD_DIR)/link_test_arm64

deploy: $(BUILD_DIR)/link_test_arm64
	scp $(BUILD_DIR)/link_test_arm64 $(BOARD):/tmp/

clean:
	rm -rf $(BUILD_DIR) *.plist

.PHONY: all test coverage analyze image board deploy clean
