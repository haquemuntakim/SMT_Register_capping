# SMT Register Capping Makefile

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
SRCDIR = src
TESTDIR = tests
OBJDIR = build
BINDIR = bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Test files
TEST_SOURCES = $(wildcard $(TESTDIR)/*.cpp)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/%.cpp=$(BINDIR)/%)

# Example files
EXAMPLE_SOURCES = $(wildcard examples/*.cpp)
EXAMPLE_TARGETS = $(EXAMPLE_SOURCES:examples/%.cpp=$(BINDIR)/%)

# Default target
all: directories $(TEST_TARGETS) $(EXAMPLE_TARGETS)

# Create necessary directories
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | directories
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build test executables
$(BINDIR)/test_%: $(TESTDIR)/test_%.cpp $(OBJECTS) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

# Build example executables
$(BINDIR)/%: examples/%.cpp $(OBJECTS) | directories
	$(CXX) $(CXXFLAGS) $^ -o $@

# Run tests
test: $(BINDIR)/test_smt_register_allocator
	@echo "Running SMT Register Allocator Tests..."
	@./$(BINDIR)/test_smt_register_allocator

# Run simulation example
simulate: $(BINDIR)/smt_simulation
	@echo "Running SMT Register Capping Simulation..."
	@./$(BINDIR)/smt_simulation

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(BINDIR)

# Debug build
debug: CXXFLAGS += -DDEBUG -O0
debug: all

# Release build
release: CXXFLAGS += -DNDEBUG -O3
release: all

# Install (placeholder for system installation)
install: all
	@echo "Installation not implemented yet"

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build all targets (default)"
	@echo "  test     - Build and run tests"
	@echo "  simulate - Build and run simulation example"
	@echo "  clean    - Remove build artifacts"
	@echo "  debug    - Build with debug flags"
	@echo "  release  - Build with release optimizations"
	@echo "  help     - Show this help message"

# Phony targets
.PHONY: all directories test simulate clean debug release install help

# Dependencies
$(OBJDIR)/smt_register_allocator.o: $(SRCDIR)/smt_register_allocator.h
$(BINDIR)/test_smt_register_allocator: $(OBJDIR)/smt_register_allocator.o