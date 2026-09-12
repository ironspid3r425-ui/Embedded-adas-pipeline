# Host build of the portable diagnostic logic + Unity tests.
# No hardware, no cross-compiler: runs on any machine with gcc.
#   make test   - build and run the unit tests
#   make clean  - remove build output

CC      ?= gcc
CFLAGS  := -std=c99 -Wall -Wextra -Werror -Isrc -Ivendor/unity
BUILD   := build
SRC     := src/diag.c
UNITY   := vendor/unity/unity.c
TEST    := test/test_diag.c

.PHONY: test clean
test: $(BUILD)/test_diag
	./$(BUILD)/test_diag

$(BUILD)/test_diag: $(SRC) $(UNITY) $(TEST) | $(BUILD)
	$(CC) $(CFLAGS) $(SRC) $(UNITY) $(TEST) -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
