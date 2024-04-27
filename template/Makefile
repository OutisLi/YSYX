BUILD_DIR = ./build
TOP = top
MAIN = Elaborate
OBJ_DIR = $(BUILD_DIR)/obj_dir
LOG_DIR = $(BUILD_DIR)/log
TOPNAME = top
TOP_V = $(BUILD_DIR)/$(TOPNAME).sv

SCALA_FILE = $(shell find ./src/main -name "*.scala")

VERILATOR = verilator
VERILATOR_COVERAGE = verilator_coverage
VERILATOR_FLAGS += -MMD --trace --build -cc --exe --build -j 0 \
                                     -O3 --x-assign fast --x-initial fast --noassert -report-unoptflat

# timescale set
VERILATOR_FLAGS += --timescale 1ps/1ps

# C flags
INC_PATH += $(abspath ./sim_c/include)
INCFLAGS = $(addprefix -I, $(INC_PATH))
CFLAGS += $(INCFLAGS) $(CFLAGS_SIM) -DTOP_NAME="V$(TOPNAME)"
# add vcd
CFLAGS += -DVCD

# source file
VSRCS = $(TOP_V)
CSRCS = $(shell find $(abspath ./sim_c) -name "*.c" -or -name "*.cc" -or -name "*.cpp")

BIN = $(BUILD_DIR)/$(TOP)
NPC_EXEC := $(BIN)

$(TOP_V): $(SCALA_FILE)
	$(call git_commit, "generate verilog")
	@mkdir -p $(@D)
	mill $(TOP).runMain $(MAIN) --target-dir $(@D) --throw-on-first-error --full-stacktrace

test:
	mill -i $(TOP).test

verilog: $(TOP_V)

help:
	mill -i $(TOP).runMain Elaborate --help

reformat:
	mill -i __.reformat

checkformat:
	mill -i __.checkFormat

clean:
	-rm -rf $(BUILD_DIR)

clean_mill:
	-rm -rf out

clean_all: clean clean_mill

sim: $(CSRCS) $(VSRCS)
	$(call git_commit, "sim RTL") # DO NOT REMOVE THIS LINE!!!
	@rm -rf $(OBJ_DIR)
	$(VERILATOR) $(VERILATOR_FLAGS) -top $(TOPNAME) $^ \
        $(addprefix -CFLAGS , $(CFLAGS)) $(addprefix -LDFLAGS , $(LDFLAGS)) \
        --Mdir $(OBJ_DIR) -o $(abspath $(BIN))

run:
	@echo
	@echo "------------ RUN --------------"
	$(NPC_EXEC)
	@mkdir -p $(LOG_DIR)
	@cp waveform.vcd $(LOG_DIR)
	@rm -f waveform.vcd
	@echo "the simulation log is in $(LOG_DIR)"

run_all: verilog sim run
	@echo "use 'make showwave' to show waveform"

showwave:
	gtkwave $(LOG_DIR)/waveform.vcd

-include ../Makefile

.PHONY: test verilog help reformat checkformat clean clean_mill clean_all sim run run_all showwave
