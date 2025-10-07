# Nuke built-in rules and variables.
MAKEFLAGS += -rR
.SUFFIXES:

# Architecture (default to x86_64)
ARCH := x86_64

# QEMU flags with ac97 sound card and 1920x1080 resolution
QEMUFLAGS := -m 5G -serial stdio -device ac97 -device VGA,xres=1920,yres=1080

# Image name
override IMAGE_NAME := template-$(ARCH)

# Host toolchain
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

.PHONY: run
run: run-$(ARCH)

.PHONY: run-hdd
run-hdd: run-hdd-$(ARCH)

.PHONY: tools
tools:
	$(MAKE) -C kernel/src/kernel/tools

.PHONY: ide-image
ide-image:
	@if [ ! -f ide.img ]; then \
		echo "Creating ide.img (8MB raw disk image for fat12)..."; \
		qemu-img create -f raw ide.img 8M; \
		echo "Formatting as FAT12..."; \
		sudo mkfs.fat -F 12 -v -n "TESTDISK" ide.img ::; \
		if [ -d test ]; then \
			echo "Copying test files to ide.img..."; \
			mmd -i ide.img ::/test 2>/dev/null || true; \
			for file in test/*; do \
				if [ -f "$$file" ]; then \
					echo "Copying $$file..."; \
					mcopy -i ide.img "$$file" ::/test/; \
				fi; \
			done; \
		fi; \
	fi

.PHONY: ahci-image
ahci-image:
	@if [ ! -f ahci.img ]; then \
		echo "Creating ahci.img (32MB raw disk image)..."; \
		qemu-img create -f raw ahci.img 32M; \
		echo "Formatting as FAT12..."; \
		mformat -F -v "AHCIDISK" -i ahci.img ::; \
		if [ -d test ]; then \
			echo "Copying test files to ahci.img..."; \
			mmd -i ahci.img ::/test 2>/dev/null || true; \
			for file in test/*; do \
				if [ -f "$$file" ]; then \
					echo "Copying $$file..."; \
					mcopy -i ahci.img "$$file" ::/test/; \
				fi; \
			done; \
		fi; \
	fi

# Add a target to rebuild images with test files
.PHONY: rebuild-images
rebuild-images:
	rm -f ide.img ahci.img
	$(MAKE) ide-image ahci-image

.PHONY: run-x86_64
run-x86_64: clean $(IMAGE_NAME).iso ide-image ahci-image
	clear
	qemu-system-x86_64 \
		-M pc \
		$(QEMUFLAGS) \
		-cdrom $(IMAGE_NAME).iso \
		-drive file=ide.img,format=raw,if=none,id=drive0 \
		-device ide-hd,drive=drive0,bus=ide.0,unit=0 \
		-drive file=ahci.img,format=raw,if=none,id=drive1 \
		-device ahci,id=ahci \
		-device ide-hd,drive=drive1,bus=ahci.0 \
		-device piix3-usb-uhci,id=uhci \
		-no-reboot -no-shutdown -s

.PHONY: run-aarch64
run-aarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).iso
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-hdd-aarch64
run-hdd-aarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).hdd
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu cortex-a72 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-riscv64
run-riscv64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).iso
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-hdd-riscv64
run-hdd-riscv64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).hdd
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu rv64 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-loongarch64
run-loongarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).iso
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-hdd-loongarch64
run-hdd-loongarch64: ovmf/ovmf-code-$(ARCH).fd $(IMAGE_NAME).hdd
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M virt \
		-cpu la464 \
		-device ramfb \
		-device qemu-xhci \
		-device usb-kbd \
		-device usb-mouse \
		-drive if=pflash,unit=0,format=raw,file=ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: codespace-setup
codespace-setup:
	@echo "Installing build dependencies for GitHub Codespaces..."
	sudo apt update && sudo apt install -y build-essential nasm qemu-system-x86 qemu-utils mtools xorriso sgdisk curl

.PHONY: run-codespace
run-codespace: clean $(IMAGE_NAME).iso ide-image ahci-image
	@echo "Running QEMU in headless mode for Codespaces..."
	qemu-system-x86_64 \
		-m 2G \
		-cdrom $(IMAGE_NAME).iso \
		-drive file=ide.img,format=raw,if=none,id=drive0 \
		-device ide-hd,drive=drive0,bus=ide.0,unit=0 \
		-drive file=ahci.img,format=raw,if=none,id=drive1 \
		-device ahci,id=ahci \
		-device ide-hd,drive=drive1,bus=ahci.0 \
		-no-reboot \
		-no-shutdown \
		-nographic \
		-s

.PHONY: run-bios
run-bios: $(IMAGE_NAME).iso
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

.PHONY: run-hdd-bios
run-hdd-bios: $(IMAGE_NAME).hdd
	QEMU_AUDIO_DRV=pa qemu-system-$(ARCH) \
		-M q35 \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

ovmf/ovmf-code-$(ARCH).fd:
	mkdir -p ovmf
	curl -Lo $@ https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/ovmf-code-$(ARCH).fd
	case "$(ARCH)" in \
		aarch64) dd if=/dev/zero of=$@ bs=1 count=0 seek=67108864 2>/dev/null;; \
		riscv64) dd if=/dev/zero of=$@ bs=1 count=0 seek=33554432 2>/dev/null;; \
	esac

limine/limine:
	rm -rf limine
	git clone https://github.com/limine-bootloader/limine.git --branch=v9.x-binary --depth=1
	$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

kernel-deps:
	./kernel/get-deps
	touch kernel-deps

.PHONY: kernel
kernel: kernel-deps
	$(MAKE) -C kernel

$(IMAGE_NAME).iso: limine/limine kernel
	rm -rf iso_root
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	cp -v kernel/bin-$(ARCH)/kernel iso_root/boot/
	cp -v limine.conf iso_root/boot/limine/
ifeq ($(ARCH),x86_64)
	cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),aarch64)
	cp -v limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTAA64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),riscv64)
	cp -v limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTRISCV64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
ifeq ($(ARCH),loongarch64)
	cp -v limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTLOONGARCH64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J \
		-hfsplus -apm-block-size 2048 \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
endif
	rm -rf iso_root

$(IMAGE_NAME).hdd: limine/limine kernel
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
ifeq ($(ARCH),x86_64)
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine/limine bios-install $(IMAGE_NAME).hdd
else
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
endif
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin-$(ARCH)/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf ::/boot/limine
ifeq ($(ARCH),x86_64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),aarch64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTAA64.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),riscv64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTRISCV64.EFI ::/EFI/BOOT
endif
ifeq ($(ARCH),loongarch64)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTLOONGARCH64.EFI ::/EFI/BOOT
endif

.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd ide.img ahci.img

.PHONY: distclean
distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root *.iso *.hdd kernel-deps limine ovmf ide.img ahci.img