LIB = storpu
SRCS = errno.c memcpy.S memset.S sbrk.c mmap.c malloc.c fs.c

CROSS = /usr/local/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
CC = $(CROSS)gcc
AS = $(CROSS)as
AR = ar

CFLAGS = -fPIC -I../include
ARFLAGS = rcs

SRC_PATH = ./src
BUILD_PATH = ./obj
OBJS = $(patsubst %.c, $(BUILD_PATH)/%.o, $(patsubst %.S, $(BUILD_PATH)/%.o, $(patsubst %.asm, $(BUILD_PATH)/%.o, $(SRCS))))
DEPS = $(OBJS:.o=.d)
Q = @

BIN = $(BUILD_PATH)/lib$(LIB).a

.PHONY : storpu clean

storpu : $(BUILD_PATH) $(BIN)
	@true

clean :
	$(Q)rm -rf ./obj

$(BIN): $(OBJS) $(EXTRAOBJS)
	@echo -e '\tAR\t$@'
	$(Q)$(AR) $(ARFLAGS) $@ $(OBJS) $(EXTRAOBJS)

$(BUILD_PATH):
	$(Q)mkdir $(BUILD_PATH)

-include $(DEPS)

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.c
	@echo -e '\tCC\t$<'
	$(Q)$(CC) $(CFLAGS) -c -MP -MMD $< -o $@

$(BUILD_PATH)/%.o: $(SRC_PATH)/%.S
	@echo -e '\tAS\t$<'
	$(Q)$(CC) $(CFLAGS) -c -D__ASSEMBLY__ -o $@ $<
