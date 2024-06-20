# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 NVIDIA Corporation
# Modified by Meili Authors 
# Copyright (c) 2024, Meili Authors 

# binary name
APP = meili

# all source are stored in SRCS-y
SRCS-ALL := $(shell find ./src -type f -name '*.c')

SRCS-y := $(SRCS-ALL)

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

all: static
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)

LDFLAGS += -lhs -lpcap -lstdc++ -lrxp_compiler
CFLAGS += -I/usr/local/include/hs
CFLAGS += -I/usr/include/hs

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 -g $(shell $(PKGCONF) --cflags libdpdk)
#CFLAGS += -O3 -g -time -ftime-report $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_SHARED := $(filter-out -lrte_reorder,$(LDFLAGS_SHARED))

LDFLAGS_STATIC := -L/opt/mellanox/dpdk/lib/aarch64-linux-gnu \
-Wl,--whole-archive -l:librte_bus_auxiliary.a -l:librte_bus_pci.a -l:librte_bus_vdev.a -l:librte_common_mlx5.a \
-l:librte_mempool_bucket.a -l:librte_mempool_ring.a -l:librte_mempool_stack.a -l:librte_net_af_packet.a \
-l:librte_net_mlx5.a  -l:librte_net_virtio.a -l:librte_compress_mlx5.a -l:librte_regex_mlx5.a \
-l:librte_regexdev.a -l:librte_compressdev.a \
-l:librte_acl.a \
-l:librte_pci.a -l:librte_ethdev.a -l:librte_stack.a \
-l:librte_net.a -l:librte_mbuf.a -l:librte_mempool.a -l:librte_ring.a -l:librte_eal.a -l:librte_kvargs.a \
-l:librte_telemetry.a -l:librte_hash.a -l:librte_ip_frag.a -l:librte_rcu.a -l:librte_lpm.a \
-Wl,--no-whole-archive -Wl,--export-dynamic -lmtcr_ul -lmlx5 -lpthread -libverbs \
-lnl-route-3 -lnl-3 -lelf -lz -lpcap -ljansson \
-Wl,--as-needed -lm -ldl -lnuma -lpthread


CFLAGS += -I/opt/mellanox/doca/include
DOCA_LIB_DIR := $(shell $(PKGCONF) --libs-only-L doca-regex)
LDFLAGS_STATIC += -lstdc++ -lbsd -ljson-c
CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -DDOCA_ALLOW_EXPERIMENTAL_API
CFLAGS += -DUSE_HYPERSCAN

GIT_VERSION := "$(shell git rev-parse --short HEAD || echo "release")"
CFLAGS += -DGIT_SHA=\"$(GIT_VERSION)\"

build/$(APP)-shared: $(SRCS-y) Makefile $(PC_FILE) | build
	@/bin/echo ' ' CC $<
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(SRCS-y) Makefile $(PC_FILE) | build
	@/bin/echo ' ' CC $<
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP) build/$(APP)-static build/$(APP)-shared
	test -d build && rmdir -p build || true
