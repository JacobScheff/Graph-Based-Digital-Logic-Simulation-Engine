# Portable build wrapper for WSL/Linux.
# GNU Make reads this file before the CMake-generated Makefile, so paths
# stay relative to this directory instead of going stale across machines.

CMAKE ?= cmake
BUILD_DIR := .
SOURCE_DIR := $(abspath ..)

.PHONY: all configure clean rebuild

all: configure
	@$(CMAKE) --build $(BUILD_DIR)

configure:
	@need=0; \
	if [ ! -f $(BUILD_DIR)/CMakeCache.txt ]; then \
		need=1; \
	else \
		cached=$$(grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' $(BUILD_DIR)/CMakeCache.txt | cut -d= -f2-); \
		if [ "$$cached" != "$(SOURCE_DIR)" ]; then need=1; fi; \
	fi; \
	if [ "$$need" = "1" ]; then \
		if [ -f $(BUILD_DIR)/CMakeCache.txt ]; then \
			echo "Removing stale CMake cache..."; \
			rm -f $(BUILD_DIR)/CMakeCache.txt; \
			rm -rf $(BUILD_DIR)/CMakeFiles $(BUILD_DIR)/cmake_install.cmake; \
		fi; \
		echo "Configuring CMake for $(SOURCE_DIR)..."; \
		$(CMAKE) -S $(SOURCE_DIR) -B $(BUILD_DIR); \
	fi

clean:
	@if [ -f $(BUILD_DIR)/CMakeCache.txt ]; then \
		$(CMAKE) --build $(BUILD_DIR) --target clean; \
	else \
		echo "Nothing to clean."; \
	fi

rebuild: clean all
