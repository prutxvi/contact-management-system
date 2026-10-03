# Contact Management System -- Group 11
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
BIN      := cms
DATA     := contacts.csv

.PHONY: all run test bench demo clean fmt

all: $(BIN)

$(BIN): src/contact_manager.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

bench: bench/bench.cpp src/contact_manager.cpp
	$(CXX) $(CXXFLAGS) -o bench_bin bench/bench.cpp

run: $(BIN)
	./$(BIN) --data $(DATA)

test: $(BIN)
	python3 tests/test_cms.py

# Recompute docs/benchmarks.md from this machine.
measure: bench
	./bench_bin

demo: $(BIN)
	./demo/run_demo.sh

# Writes dist/Group11_ContactManagementSystem.zip: the single source file, the report,
# a saved sample run, and a short build/run note.
package: $(BIN)
	rm -rf submission dist && mkdir -p submission dist
	cp src/contact_manager.cpp submission/Group11_ContactManagementSystem.cpp
	cp docs/report.md submission/Group11_Report.md
	cp demo/session.log submission/Group11_sample_output.txt
	cp docs/submission_note.md submission/HOW_TO_RUN.md
	cd dist && cp -R ../submission Group11_ContactManagementSystem && \
	  zip -qr Group11_ContactManagementSystem.zip Group11_ContactManagementSystem && \
	  rm -rf Group11_ContactManagementSystem
	@echo "wrote dist/Group11_ContactManagementSystem.zip"

clean:
	rm -f $(BIN) bench_bin $(DATA) $(DATA).tmp
	rm -rf demo/out demo/scratch
