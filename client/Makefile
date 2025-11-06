# INSEN Client Makefile
# Builds C client library and examples

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 
TARGET = insen_example
SOURCES = example.c insen_client.c
HEADERS = insen_client.h

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    # Linux settings
    CFLAGS += -D_DEFAULT_SOURCE
endif
ifeq ($(UNAME_S),Darwin)
    # macOS settings
    CFLAGS += -D_DARWIN_C_SOURCE
endif

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

# Build library only
lib: insen_client.o

insen_client.o: insen_client.c insen_client.h
	$(CC) $(CFLAGS) -c -o $@ insen_client.c

# Install library (Unix-like systems)
install: lib
	sudo mkdir -p /usr/local/include
	sudo mkdir -p /usr/local/lib
	sudo cp insen_client.h /usr/local/include/
	sudo cp insen_client.o /usr/local/lib/libinsen_client.a

# Clean build files
clean:
	rm -f $(TARGET) *.o

# Test with virtual port (Linux only)
test-virtual:
	@echo "Setting up virtual serial port pair..."
	socat -d -d pty,raw,echo=0 pty,raw,echo=0 &
	@echo "Use the displayed port names to test the client"

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build
release: CFLAGS += -DNDEBUG
release: $(TARGET)

# Static analysis
analyze:
	cppcheck --enable=all --std=c99 $(SOURCES)

# Format code
format:
	clang-format -i $(SOURCES) $(HEADERS)

# Show usage
help:
	@echo "INSEN Client Build System"
	@echo "========================="
	@echo "Targets:"
	@echo "  all        - Build example program (default)"
	@echo "  lib        - Build library only"
	@echo "  install    - Install library system-wide (Unix)"
	@echo "  clean      - Remove build files"
	@echo "  debug      - Build with debug symbols"
	@echo "  release    - Build optimized release"
	@echo "  analyze    - Run static analysis (requires cppcheck)"
	@echo "  format     - Format source code (requires clang-format)"
	@echo "  test-virtual - Set up virtual serial ports for testing (Linux)"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  make"
	@echo "  ./$(TARGET) /dev/ttyUSB0"
	@echo "  ./$(TARGET) COM3"

.PHONY: all lib install clean test-virtual debug release analyze format help