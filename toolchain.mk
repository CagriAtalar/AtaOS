# toolchain.mk - compiler, linker and flag configuration (ADR-0003).
#
# Strategy: clang + ld.lld targeting x86_64-unknown-none-elf. Override on the
# command line, e.g.:  make CC=clang-18 LD=ld.lld-18
# Every non-default flag below has a documented reason.

ifeq ($(origin CC),default)
  CC := clang
endif
ifeq ($(origin LD),default)
  LD := ld.lld
endif
CC      ?= clang
LD      ?= ld.lld
HOST_CC ?= cc

TARGET  := x86_64-unknown-none-elf

VERSION := 0.1.0
MODE    ?= debug

ifeq ($(MODE),debug)
  OPT_FLAGS := -O0 -g -DATAOS_DEBUG
  WERROR    := -Werror
else ifeq ($(MODE),release)
  OPT_FLAGS := -O2 -g -DNDEBUG
  WERROR    :=
else
  $(error MODE must be 'debug' or 'release' (got '$(MODE)'))
endif

# Configuration flavour (separate build directories, see Makefile).
TEST       ?= 0
TEST_PANIC ?= 0
TEST_FAULT ?= 0
ifeq ($(TEST_PANIC),1)
  TEST := 1
endif
ifeq ($(TEST_FAULT),1)
  TEST_FAULT := ud
endif
ifneq ($(TEST_FAULT),0)
  TEST := 1
endif

CONFIG_NAME := $(MODE)
ifeq ($(TEST),1)
  CONFIG_NAME := $(MODE)-test
  FLAVOR_DEFS := -DATAOS_TEST_BUILD
endif
ifeq ($(TEST_PANIC),1)
  CONFIG_NAME := $(MODE)-testpanic
  FLAVOR_DEFS += -DATAOS_TEST_PANIC
endif
ifneq ($(TEST_FAULT),0)
  CONFIG_NAME := $(MODE)-testfault-$(TEST_FAULT)
  ifeq ($(TEST_FAULT),de)
    FLAVOR_DEFS += -DATAOS_TEST_FAULT_DE
  else ifeq ($(TEST_FAULT),ud)
    FLAVOR_DEFS += -DATAOS_TEST_FAULT_UD
  else ifeq ($(TEST_FAULT),gp)
    FLAVOR_DEFS += -DATAOS_TEST_FAULT_GP
  else ifeq ($(TEST_FAULT),pf)
    FLAVOR_DEFS += -DATAOS_TEST_FAULT_PF
  else ifeq ($(TEST_FAULT),df)
    FLAVOR_DEFS += -DATAOS_TEST_FAULT_DF
  else
    $(error Unknown TEST_FAULT='$(TEST_FAULT)'; supported: de, ud, gp, pf, df)
  endif
endif

# Warnings are bugs: never disabled to make a build pass.
WARN_FLAGS := -Wall -Wextra -Wshadow -Wvla -Wstrict-prototypes -Wmissing-prototypes \
              -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings -Wno-unused-parameter

# -ffreestanding            no hosted-environment assumptions
# -fno-builtin              we implement mem*/str*; stop loop->memcpy self-recursion
# -std=gnu11                ADR-0002
# -mcmodel=kernel           top-2-GiB link address (ADR-0005)
# -mno-red-zone             interrupts would clobber the red zone below RSP
# -mgeneral-regs-only etc.  no x87/MMX/SSE in the kernel (no FPU state saving yet)
# -fno-pic -fno-pie         fixed-address image, no relocations at load
# -fno-stack-protector      no guard-variable runtime yet (revisit at M4)
# -fno-stack-check          no probes (no libc support)
# -fno-omit-frame-pointer   stack walking / GDB
# -fno-lto                  simple, debuggable output
# -ffunction/data-sections  with --gc-sections; request sections are KEEPed in the linker script
# -ffile-prefix-map         reproducible paths in debug info and __FILE__
ARCH_FLAGS := -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
              -mno-80387 -mgeneral-regs-only
CODEGEN_FLAGS := -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector \
                 -fno-stack-check -fno-omit-frame-pointer -fno-lto -fno-common \
                 -ffunction-sections -fdata-sections \
                 -ffile-prefix-map=$(CURDIR)=.

INCLUDES = -Iinclude -Iarch/x86_64/include -Ithird_party/limine -I$(GEN_DIR)

CFLAGS  = --target=$(TARGET) -std=gnu11 $(ARCH_FLAGS) $(CODEGEN_FLAGS) $(WARN_FLAGS) $(WERROR) \
          $(OPT_FLAGS) $(FLAVOR_DEFS) $(INCLUDES) -MMD -MP
ASFLAGS = --target=$(TARGET) $(ARCH_FLAGS) -g $(FLAVOR_DEFS) $(INCLUDES) \
          -ffile-prefix-map=$(CURDIR)=. -MMD -MP

# -nostdlib -static          no libc / startfiles / dynamic loader
# -z max-page-size=0x1000    4 KiB-aligned segments (Limine maps by page)
# -z noexecstack             no executable stack
# --gc-sections              drop unused sections (requests are KEEPed)
# --build-id=none            reproducibility
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -z noexecstack --gc-sections \
           --build-id=none -T arch/x86_64/linker.ld
