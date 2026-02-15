IMAGE ?= distro-init
CC ?= cc
CFLAGS ?= -O2 -pipe -Wall
LDFLAGS ?=
LDLIBS ?=

SRC_DIR := .
ROOTFS := rootfs

SBINS := init login adduser gpasswd kill shutdown
BINS := sh echo ls cat env sleep segfault whoami sudo gpasswd

.PHONY: all build rootfs docker-build docker-run clean

all: rootfs

SBIN_TARGETS := $(addprefix $(ROOTFS)/usr/sbin/,$(SBINS))
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
	@chmod u+s $(ROOTFS)/usr/bin/sudo

Dockerfile: ;

docker-build: rootfs
	docker build -t $(IMAGE) .

docker-run: docker-build
	docker run -ti --rm $(IMAGE)

clean:
	rm -rf $(ROOTFS)
	-@docker rmi -f $(IMAGE) >/dev/null 2>&1 || true
