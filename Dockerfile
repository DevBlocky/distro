FROM scratch
COPY rootfs/ /
ENTRYPOINT ["/usr/sbin/init"]
