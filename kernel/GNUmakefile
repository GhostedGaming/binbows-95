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
	OBJCOPY := llvm-objcopy
else
	ifneq ($(TOOLCHAIN_PREFIX),)
		CC := $(TOOLCHAIN_PREFIX)gcc
		LD := $(TOOLCHAIN_PREFIX)ld
		OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
	else
		CC := cc
		LD := ld
		OBJCOPY := objcopy
	endif
endif

NASM := nasm

# Compiler and linker flags
CFLAGS := -g -O2 -pipe
CPPFLAGS :=
LDFLAGS :=

ifeq ($(ARCH),x86_64)
	NASMFLAGS := -F dwarf -g -Wall -f elf64 -Wno-deprecated
	CFLAGS += \
		-Wall -Wextra -std=gnu11 -nostdinc -ffreestanding \
		-fno-stack-protector -fno-stack-check -fno-lto -fno-PIC \
		-ffunction-sections -fdata-sections \
		-m64 -march=x86-64 -mabi=sysv -mno-80387 -mno-mmx \
		-mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -pedantic-errors
	LDFLAGS += -m elf_x86_64
	CPPFLAGS += \
		-I src \
		-I include/x86_64 \
		-I include/kernel \
		-I include/kernel/lib \
		-I include/kernel/fs \
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
TOOLS_ELF := $(patsubst $(TOOLS_SRCDIR)/%.c,$(TOOLS_OUTDIR)/%.elf,$(TOOLS_SRC))

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

# First create ELF, then convert to flat binary
$(TOOLS_OUTDIR)/%.elf: obj-$(ARCH)/$(TOOLS_SRCDIR)/%.c.o $(TOOLS_DEPS) $(TOOLS_LINKER_SCRIPT)
	@echo "Linking tool ELF $@ ..."
	@mkdir -p $(dir $@)
	@$(LD) -nostdlib -static -T $(TOOLS_LINKER_SCRIPT) -o $@ $(filter %.o,$^)

$(TOOLS_OUTDIR)/%.bin: $(TOOLS_OUTDIR)/%.elf
	@echo "Converting to flat binary $@ ..."
	@$(OBJCOPY) -O binary $< $@
	@echo "Generated flat binary: $@ (size: $$(stat -c%s $@) bytes)"

tools: $(TOOLS_BIN)

bin/includes: $(HEADER_FILES)

$(INCLUDE_BIN_DIR)/%.h: bin-$(ARCH)/tools/%.bin
	@mkdir -p $(INCLUDE_BIN_DIR)
	@echo "Generating header for $< ..."
	@xxd -i $< > $@

clean:
	@echo "Removing binaries, objects, and generated headers..."
	@rm -rf bin-$(ARCH) obj-$(ARCH) $(INCLUDE_BIN_DIR)

# Clean intermediate ELF files but keep final binaries
clean-intermediate:
	@echo "Removing intermediate ELF files..."
	@rm -f $(TOOLS_ELF)

install: all
	@echo "Installing $(OUTPUT) to $(DESTDIR)$(PREFIX)/share/$(OUTPUT)/ ..."
	@install -d "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"
	@install -m 644 bin-$(ARCH)/$(OUTPUT) "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"

uninstall:
	@echo "Uninstalling $(OUTPUT) from $(DESTDIR)$(PREFIX)/share/$(OUTPUT)/ ..."
	@rm -f "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)/$(OUTPUT)-$(ARCH)"
	@-rmdir "$(DESTDIR)$(PREFIX)/share/$(OUTPUT)"

# Debugging targets
debug-tools:
	@echo "Tools source files: $(TOOLS_SRC)"
	@echo "Tools ELF files: $(TOOLS_ELF)"
	@echo "Tools binary files: $(TOOLS_BIN)"
	@echo "Header files: $(HEADER_FILES)"

verify-binaries: $(TOOLS_BIN)
	@echo "Verifying flat binaries..."
	@for bin in $(TOOLS_BIN); do \
		echo "Checking $$bin:"; \
		file $$bin; \
		echo "Size: $$(stat -c%s $$bin) bytes"; \
		echo "First 32 bytes:"; \
		hexdump -C $$bin | head -n 2; \
		echo ""; \
	done