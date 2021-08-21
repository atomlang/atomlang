CC             = gcc
CFLAGS         = -fPIC
DEBUG_CFLAGS   = -D DEBUG -g3 -Og
RELEASE_CFLAGS = -g -O3
LDFLAGS        = -lm

TARGET_EXEC = atomlang
BUILD_DIR   = ./build

SRC_DIRS = ./src ./cli
INC_DIRS = ./src/include

SRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -name *.c)
OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))
DEP_FLAGS  = -MMD -MP
CC_FLAGS   = $(INC_FLAGS) $(DEP_FLAGS) $(CFLAGS)

DEBUG_DIR     = $(BUILD_DIR)/debug
DEBUG_TARGET  = $(DEBUG_DIR)/$(TARGET_EXEC)
DEBUG_OBJS   := $(addprefix $(DEBUG_DIR)/, $(OBJS))

RELEASE_DIR     = $(BUILD_DIR)/release
RELEASE_TARGET  = $(RELEASE_DIR)/$(TARGET_EXEC)
RELEASE_OBJS   := $(addprefix $(RELEASE_DIR)/, $(OBJS))

.PHONY: debug release all clean

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(DEBUG_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(DEBUG_CFLAGS) -c $< -o $@

release: $(RELEASE_TARGET)

$(RELEASE_TARGET): $(RELEASE_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

$(RELEASE_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(RELEASE_CFLAGS) -c $< -o $@

all: debug release

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
