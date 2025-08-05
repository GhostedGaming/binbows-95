# Top-level Makefile for kernel + tools
MAKEFLAGS += -rR
.SUFFIXES:

OUTPUT := kernel
ARCH := x86_64
PREFIX := /usr/local

TOOLCHAIN :=
TOOLCHAIN_PREFIX :=

ifeq ($(TOOLCHAIN),llvm)
	CC := clang
	LD := ld.lld
else
	ifneq ($(TOOLCHAIN_PREFIX),)
		CC := $(TOOLCHAIN_PREFIX)gcc
		LD := $(TOOLCHAIN_PREFIX)ld
	else
		CC := cc
		LD := ld
	endif
endif

NASM := nasm

# Compiler and linker flags
CFLAGS := -g -O2 -pipe
CPPFLAGS :=
LDFLAGS :=

ifeq ($(ARCH),x86_64)
	NASMFLAGS := -F dwarf -g -Wall -f elf64 -Wno-deprecated # silence some warnings
	CFLAGS += \
		-Wall -Wextra -std=gnu11 -nostdinc -ffreestanding \
		-fno-stack-protector -fno-stack-check -fno-lto -fno-PIC \
		-ffunction-sections -fdata-sections \
		-m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx \
		-mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel
	LDFLAGS += -m elf_x86_64
	CPPFLAGS += \
		-I src \
		-I include/arch/x86_64 \
		-I include/kernel \
        -I bin/includes \
		-I limine-protocol/include \
		-isystem freestnd-c-hdrs/include \
		-DLIMINE_API_REVISION=3 -MMD -MP
	LINKER_SCRIPT := linker-scripts/$(ARCH).lds
	TOOLS_LINKER_SCRIPT := linker-scripts/tools-$(ARCH).lds
endif

# Source files
SRCFILES := $(shell find -L src cc-runtime/src -type f)
CFILES := $(filter %.c,$(SRCFILES))
ASFILES := $(filter %.S,$(SRCFILES))
NASMFILES := $(filter %.asm,$(SRCFILES))

OBJ := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o) $(NASMFILES:.asm=.asm.o))
HEADER_DEPS := $(addprefix obj-$(ARCH)/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

# Tools sources and objects
TOOLS_SRCDIR := src/kernel/tools
TOOLS_OUTDIR := bin-$(ARCH)/tools
TOOLS_SRC := $(wildcard $(TOOLS_SRCDIR)/*.c)
TOOLS_OBJ := $(patsubst %.c,obj-$(ARCH)/%.o,$(TOOLS_SRC))
TOOLS_BIN := $(patsubst $(TOOLS_SRCDIR)/%.c,$(TOOLS_OUTDIR)/%.bin,$(TOOLS_SRC))

# Additional dependencies for tools - e.g. shared libs
TOOLS_DEPS := obj-$(ARCH)/src/drivers/char/serial.c.o

# Directory to output generated headers from binaries
INCLUDE_BIN_DIR := bin/includes

# Generate header filenames from binary tools
BIN_FILES := $(TOOLS_BIN)
HEADER_FILES := $(patsubst bin-$(ARCH)/tools/%.bin,$(INCLUDE_BIN_DIR)/%.h,$(BIN_FILES))

.PHONY: all autoclean clean distclean install uninstall tools bin/includes

# Fix build order: tools + headers before kernel!
all: autoclean tools bin/includes bin-$(ARCH)/$(OUTPUT)

autoclean:
	@echo "Auto-cleaning before build..."
	@rm -rf bin-$(ARCH) obj-$(ARCH) $(INCLUDE_BIN_DIR)

-include $(HEADER_DEPS)

bin-$(ARCH)/$(OUTPUT): $(LINKER_SCRIPT) $(OBJ) bin/includes
	@echo "Linking final executable $@ ..."
	@mkdir -p $(dir $@)
	@$(LD) $(OBJ) $(LDFLAGS) -nostdlib -static -z max-page-size=0x1000 --gc-sections -T $(LINKER_SCRIPT) -o $@

obj-$(ARCH)/%.c.o: %.c GNUmakefile
	@echo "Compiling C source $< ..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

obj-$(ARCH)/%.S.o: %.S GNUmakefile
	@echo "Compiling assembly source $< ..."
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

obj-$(ARCH)/%.asm.o: %.asm GNUmakefile
	@echo "Assembling NASM source $< ..."
	@mkdir -p $(dir $@)
	@$(NASM) $(NASMFLAGS) $< -o $@

$(TOOLS_OUTDIR)/%.bin: obj-$(ARCH)/$(TOOLS_SRCDIR)/%.c.o $(TOOLS_DEPS) $(TOOLS_LINKER_SCRIPT)
	@echo "Linking tool $@ ..."
	@mkdir -p $(dir $@)
	@$(LD) -nostdlib -static -T $(TOOLS_LINKER_SCRIPT) -o $@ $(filter %.o,$^)

tools: $(TOOLS_BIN)

bin/includes: $(HEADER_FILES)

$(INCLUDE_BIN_DIR)/%.h: bin-$(ARCH)/tools/%.bin
	@mkdir -p $(INCLUDE_BIN_DIR)
	xxd -i $< > $@

clean:
	@echo "Removing binaries, objects, and generated headers..."
	@rm -rf bin-$(ARCH) obj-$(ARCH) $(INCLUDE_BIN_DIR)

install: all
	@echo "Installing $(OUTPUT) to $(DESTDIR)$(PREFIX)/share/$(OUTPUT)/ ..."
	@install -d "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	@install -m 644 bin-$(ARCH)/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"

uninstall:
	@echo "Uninstalling $(OUTPUT) from $(DESTDIR)$(PREFIX)/share/$(OUTPUT)/ ..."
	@rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"
	@-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
