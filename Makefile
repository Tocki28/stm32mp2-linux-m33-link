# Build, check and run the link test.
#   make            build both programs for this Mac, into build/
#   make test       build the test program and run it (fake channel, no hardware)
#   make analyze    static analysis of both programs: clang analyzer + cppcheck
#   make image      create the Docker build image from docker/Dockerfile (once)
#   make board      build the flight program for the STM32MP2 (64-bit ARM Linux), in Docker
#   make deploy     build the flight program for the board and copy it there (/tmp/link_test_arm64)
#   make clean      delete everything built

# Folders:
#   flight/   everything that runs on the board
#   test/     test-only code: runs on this Mac, never on the board
#   tools/    scripts the build runs
#   docker/   the 64-bit ARM Linux build environment (make image)
#   build/    built programs; make clean deletes it, make rebuilds it
FLIGHT_DIR = flight
TEST_DIR   = test
BUILD_DIR  = build

# Two programs from the same shared files:
#   link_test   FLIGHT program: flight/ only, real rpmsg channel. Runs on the board.
#   unit_test   TEST program: the shared logic from flight/ plus test/, fake channel.
#               Runs on this Mac, never on the board.
SHARED = $(FLIGHT_DIR)/echo_test.cpp $(FLIGHT_DIR)/status.cpp
FLIGHT = $(FLIGHT_DIR)/main.cpp $(FLIGHT_DIR)/rpmsg_port.cpp $(SHARED)
TEST   = $(TEST_DIR)/test_main.cpp $(TEST_DIR)/loopback_transport.cpp $(SHARED)
FLIGHT_HEADERS = $(wildcard $(FLIGHT_DIR)/*.hpp)
TEST_HEADERS   = $(wildcard $(TEST_DIR)/*.hpp)

# Which folders each build may include from. The flight build is given only flight/,
# so it cannot even find a test-only header by name.
FLIGHT_INCLUDES = -I$(FLIGHT_DIR)
TEST_INCLUDES   = -I$(FLIGHT_DIR) -I$(TEST_DIR)

# Two more guards keep test code out of the flight program, whatever route it takes
# (for example an include written with a path, like "../test/..."):
#   guard 1  test-only headers refuse to compile unless this is defined, and only the
#            test build defines it (see test/loopback_transport.hpp)
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

analyze:
	clang++ -std=c++17 --analyze -Xanalyzer -analyzer-output=text $(FLIGHT_INCLUDES) $(FLIGHT)
	clang++ -std=c++17 --analyze -Xanalyzer -analyzer-output=text $(TEST_BUILD) $(TEST_INCLUDES) $(TEST)
	cppcheck --enable=warning,style,performance,portability --std=c++17 \
	  --inline-suppr --error-exitcode=1 --quiet $(FLIGHT_INCLUDES) $(FLIGHT)
	cppcheck --enable=warning,style,performance,portability --std=c++17 \
	  --inline-suppr --error-exitcode=1 --quiet $(TEST_BUILD) $(TEST_INCLUDES) $(TEST)
	rm -f *.plist

$(BUILD_DIR)/link_test_arm64: $(FLIGHT) $(FLIGHT_HEADERS) | $(BUILD_DIR)
	docker run --rm --platform linux/arm64 -v "$(CURDIR)":/src $(IMAGE) \
	  g++ $(FLAGS) $(FLIGHT_INCLUDES) -static -o $@ $(FLIGHT)
	$(CHECK_FLIGHT) $@

image:
	docker build --platform linux/arm64 -t $(IMAGE) docker

board: $(BUILD_DIR)/link_test_arm64

deploy: $(BUILD_DIR)/link_test_arm64
	scp $(BUILD_DIR)/link_test_arm64 $(BOARD):/tmp/

clean:
	rm -rf $(BUILD_DIR) *.plist

.PHONY: all test analyze image board deploy clean
