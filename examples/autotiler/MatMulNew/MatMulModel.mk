# User Test
#------------------------------------------
MATMULBUILD_DIR     ?= $(CURDIR)/BUILD_MODEL
MATMUL_MODEL_GEN     = $(MATMULBUILD_DIR)/GenMatMul
MATMUL_SRCG 				+= $(TILER_DSP_GENERATOR_PATH)/DSP_Generators.c
MATMUL_SRC_CODE 		 = $(MATMULBUILD_DIR)/MatMulKernels.c

# Everything bellow is not application specific
TABLE_CFLAGS=-lm
CLUSTER_STACK_SIZE?=2048
CLUSTER_SLAVE_STACK_SIZE?=1024
ifeq '$(TARGET_CHIP_FAMILY)' 'GAP9'
	TOTAL_STACK_SIZE = $(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 8)
	MODEL_L1_MEMORY=$(shell expr 125000 \- $(TOTAL_STACK_SIZE))
else
	TOTAL_STACK_SIZE = $(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 7)
	MODEL_L1_MEMORY=$(shell expr 60000 \- $(TOTAL_STACK_SIZE))
endif
ifdef MODEL_L1_MEMORY
  MODEL_GEN_EXTRA_FLAGS += --L1 $(MODEL_L1_MEMORY)
endif
ifdef MODEL_L2_MEMORY
  MODEL_GEN_EXTRA_FLAGS += --L2 $(MODEL_L2_MEMORY)
endif
ifdef MODEL_L3_MEMORY
  MODEL_GEN_EXTRA_FLAGS += --L3 $(MODEL_L3_MEMORY)
endif

MATMUL_OPT_FLAG  = -DW_M1=$(W_M1)
MATMUL_OPT_FLAG += -DH_M1=$(H_M1)
MATMUL_OPT_FLAG += -DW_M2=$(W_M2)

$(MATMULBUILD_DIR):
	mkdir $(MATMULBUILD_DIR)

# Build the code generator from the model code
$(MATMUL_MODEL_GEN): | $(MATMULBUILD_DIR)
	gcc -g -o $(MATMUL_MODEL_GEN) -I$(MATMULBUILD_DIR) -I$(TILER_DSP_GENERATOR_PATH) -I$(TILER_INC) -I$(TILER_EMU_INC) $(CURDIR)/MatMulModel.c $(MATMUL_SRCG) $(TILER_LIB) $(TABLE_CFLAGS) $(MATMUL_OPT_FLAG) $(SDL_FLAGS)

# Run the code generator  kernel code
$(MATMUL_SRC_CODE): $(MATMUL_MODEL_GEN) | $(MATMULBUILD_DIR)
	$(MATMUL_MODEL_GEN) -o $(MATMULBUILD_DIR) -c $(MATMULBUILD_DIR) $(MODEL_GEN_EXTRA_FLAGS)

gen_matmul_code: $(MATMUL_SRC_CODE)

clean_matmul_code:
	rm -rf $(MATMULBUILD_DIR)

.PHONY: gen_matmul_code clean_matmul_code
