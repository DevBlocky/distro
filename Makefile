IMAGE ?= distro-init
CC ?= cc
CFLAGS ?= -O2 -pipe
LDFLAGS ?=
LDLIBS ?=

SRC_DIR := .
ROOTFS := rootfs

SBINS := init login adduser kill shutdown
BINS := sh echo ls cat env sleep segfault whoami

.PHONY: all build rootfs clean-rootfs docker-build docker-run clean

all: rootfs

SBIN_TARGETS := $(addprefix $(ROOTFS)/usr/sbin/,$(SBINS))
# login+adduser+sudo use crypt(3)
$(ROOTFS)/usr/sbin/login: LDLIBS += -lcrypt
$(ROOTFS)/usr/sbin/adduser: LDLIBS += -lcrypt
$(ROOTFS)/usr/sbin/%: %.o
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

BIN_TARGETS := $(addprefix $(ROOTFS)/usr/bin/,$(BINS))
$(ROOTFS)/usr/bin/%: %.o
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

ALL_TARGETS := $(SBIN_TARGETS) $(BIN_TARGETS)
build: $(ALL_TARGETS)

rootfs: $(ALL_TARGETS)
	@for bin in $(ALL_TARGETS); do \
		./scripts/copy-libs.sh $$bin $(ROOTFS); \
	done
	@cp -a ./etc ./$(ROOTFS)/

Dockerfile: ;

docker-build: rootfs
	docker build -t $(IMAGE) .

docker-run: docker-build
	docker run -ti --rm $(IMAGE)

clean:
	rm -rf $(ROOTFS)
	-@docker rmi -f $(IMAGE) >/dev/null 2>&1 || true
