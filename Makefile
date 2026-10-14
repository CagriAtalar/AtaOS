# AtaOS top-level Makefile (ADR-0004). Run `make help` for the target list.
# Requires GNU Make and bash; on Windows use WSL (see docs/TESTING.md).

SHELL := /bin/bash
.DEFAULT_GOAL := all

BUILD_ROOT := build

include toolchain.mk

BUILD_DIR := $(BUILD_ROOT)/$(CONFIG_NAME)
GEN_DIR   := $(BUILD_DIR)/generated
OBJ_DIR   := $(BUILD_DIR)/obj
ELF       := $(BUILD_DIR)/ataos.elf
ISO       := $(BUILD_DIR)/ataos.iso
LIMINE_DIR := $(BUILD_ROOT)/limine
LOG_DIR   := $(BUILD_ROOT)/logs

KERNEL_C := $(wildcard kernel/*/*.c) $(wildcard arch/x86_64/*/*.c)
ifneq ($(TEST),1)
  # The test-only QEMU exit hook is compiled out of ordinary builds.
  KERNEL_C := $(filter-out arch/x86_64/cpu/test_exit.c,$(KERNEL_C))
endif
KERNEL_S := $(wildcard arch/x86_64/*/*.S)
OBJS     := $(addprefix $(OBJ_DIR)/,$(KERNEL_C:.c=.o) $(KERNEL_S:.S=.o))

QEMU         ?= qemu-system-x86_64
QEMU_DISPLAY ?= none
QEMU_COMMON  := -machine q35 -m 256M -cpu qemu64 -smp 1 -no-reboot
OVMF         ?= $(firstword $(wildcard /usr/share/ovmf/OVMF.fd /usr/share/OVMF/OVMF.fd))

ENVCHECK := CC_BIN='$(firstword $(CC))' LD_BIN='$(LD)' bash scripts/env-check.sh

##@ Build

.PHONY: all
all: kernel ## Build the kernel ELF (same as 'make kernel')

.PHONY: kernel
kernel: $(ELF) ## Build build/<config>/ataos.elf (unstripped, with symbols)

.PHONY: iso
iso: $(ISO) ## Build the hybrid BIOS/UEFI ISO build/<config>/ataos.iso

.PHONY: check-build
check-build:
	@$(ENVCHECK) require build

.PHONY: buildinfo-gen
buildinfo-gen:
	@bash scripts/gen-buildinfo.sh $(GEN_DIR)/buildinfo.h $(MODE) $(CONFIG_NAME) $(VERSION)

$(GEN_DIR)/buildinfo.h: buildinfo-gen
	@:

$(OBJ_DIR)/%.o: %.c | check-build buildinfo-gen
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S | check-build buildinfo-gen
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS) arch/x86_64/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(OBJS) -o $@
	@echo "Built $@ ($(CONFIG_NAME))"

$(LIMINE_DIR)/limine: scripts/fetch-limine.sh
	@$(ENVCHECK) require iso
	@bash scripts/fetch-limine.sh $(BUILD_ROOT)

$(ISO): $(ELF) boot/limine.conf scripts/make-iso.sh $(LIMINE_DIR)/limine
	@bash scripts/make-iso.sh $(ELF) $(ISO) $(LIMINE_DIR) $(BUILD_DIR)/iso_root boot/limine.conf

-include $(OBJS:.o=.d)

##@ Run and debug (QEMU, serial on this terminal; quit with Ctrl-A X)

.PHONY: need-qemu
need-qemu:
	@$(ENVCHECK) require qemu

.PHONY: run
run: run-bios ## Alias for run-bios

.PHONY: run-bios
run-bios: $(ISO) need-qemu ## Boot the ISO in QEMU with legacy BIOS
	$(QEMU) $(QEMU_COMMON) -cdrom $(ISO) -boot d -display $(QEMU_DISPLAY) -serial mon:stdio

.PHONY: run-uefi
run-uefi: $(ISO) need-qemu ## Boot the ISO in QEMU with UEFI (OVMF)
	@[ -n "$(OVMF)" ] || { echo "error: OVMF firmware not found; install 'ovmf' or set OVMF=/path/OVMF.fd" >&2; exit 1; }
	$(QEMU) $(QEMU_COMMON) -bios $(OVMF) -cdrom $(ISO) -boot d -display $(QEMU_DISPLAY) -serial mon:stdio

.PHONY: run-debug
run-debug: $(ISO) need-qemu ## BIOS boot paused, GDB stub on tcp::1234 (see docs/DEBUGGING.md)
	@echo "GDB: gdb $(ELF) -ex 'target remote :1234'"
	$(QEMU) $(QEMU_COMMON) -cdrom $(ISO) -boot d -display $(QEMU_DISPLAY) -serial mon:stdio -s -S

##@ Tests

HOST_BIN := $(BUILD_ROOT)/host/ataos-host-tests
HOST_SRCS := $(wildcard tests/host/*.c) kernel/lib/string.c kernel/lib/fmt.c kernel/mm/bitmap.c
HOST_SAN := $(shell echo 'int main(void){return 0;}' | $(HOST_CC) -x c - -fsanitize=address,undefined -o /dev/null >/dev/null 2>&1 && echo -fsanitize=address,undefined -fno-sanitize-recover=all)
HOST_HDRS := $(wildcard include/kernel/*.h tests/host/*.h)

$(HOST_BIN): $(HOST_SRCS) $(HOST_HDRS)
	@$(ENVCHECK) require hosttest
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=gnu11 -O1 -g -fno-omit-frame-pointer -fno-builtin -Wall -Wextra -Werror \
	    -Wno-format-truncation -Wno-format-overflow \
	    -DATAOS_HOST_TEST -Iinclude -Itests/host $(HOST_SAN) $(HOST_SRCS) -o $@
	@[ -n "$(HOST_SAN)" ] || echo "note: sanitizers unavailable, host tests built WITHOUT ASan/UBSan"

.PHONY: test-host
test-host: $(HOST_BIN) ## Host unit tests (ASan+UBSan when available)
	./$(HOST_BIN)

.PHONY: test-scripts
test-scripts: ## Tests for repository scripts (commit validator, Limine checksum, dependency check)
	bash tests/scripts/test-commit-msg.sh
	bash tests/scripts/test-fetch-limine.sh
	bash scripts/check-deps.sh

.PHONY: test-qemu
test-qemu: ## Headless QEMU smoke tests (BIOS, UEFI, Panic, and Exceptions)
	@$(MAKE) --no-print-directory TEST=1 iso
	@$(MAKE) --no-print-directory TEST_PANIC=1 iso
	@$(MAKE) --no-print-directory TEST_FAULT=de iso
	@$(MAKE) --no-print-directory TEST_FAULT=ud iso
	@$(MAKE) --no-print-directory TEST_FAULT=gp iso
	@$(MAKE) --no-print-directory TEST_FAULT=pf iso
	@$(MAKE) --no-print-directory TEST_FAULT=df iso
	@$(MAKE) --no-print-directory iso
	@$(ENVCHECK) require qemu
	python3 tests/qemu/run.py --root $(CURDIR) --mode $(MODE) --log-dir $(LOG_DIR)

.PHONY: test
test: test-host test-scripts test-qemu ## All tests

.PHONY: repro-check
repro-check: ## Build twice in different directories and compare kernel ELF checksums
	@bash scripts/repro-check.sh $(MODE)

##@ Quality

.PHONY: lint
lint: ## Dependency rules, shell syntax, plus shellcheck/cppcheck when installed
	@bash scripts/lint.sh

.PHONY: format
format: ## Format C sources in place with clang-format
	@command -v clang-format >/dev/null || { echo "error: clang-format not installed (apt install clang-format)" >&2; exit 1; }
	@find kernel arch include tests/host -name '*.[ch]' -print0 | xargs -0 clang-format -i

.PHONY: format-check
format-check: ## Verify formatting without modifying files
	@command -v clang-format >/dev/null || { echo "error: clang-format not installed (apt install clang-format)" >&2; exit 1; }
	@find kernel arch include tests/host -name '*.[ch]' -print0 | xargs -0 clang-format --dry-run --Werror

.PHONY: env-check
env-check: ## Report available tools and their versions
	@$(ENVCHECK)

.PHONY: hooks
hooks: ## Install the repo-local commit-msg hook (explicit opt-in)
	@bash scripts/install-git-hooks.sh

##@ Housekeeping

.PHONY: clean
clean: ## Remove the current configuration's build directory
	rm -rf $(BUILD_DIR)

.PHONY: distclean
distclean: ## Remove all of build/ (including downloaded Limine)
	rm -rf $(BUILD_ROOT)

.PHONY: help
help: ## List targets
	@awk 'BEGIN{FS=":.*## "} /^##@/{printf "\n%s\n", substr($$0,5)} /^[a-zA-Z0-9_-]+:.*## /{printf "  %-14s %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
	@echo "Variables: MODE=debug|release  TEST=1 (test-exit hook)  TEST_PANIC=1  QEMU_DISPLAY=none|gtk  OVMF=path"
