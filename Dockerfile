FROM scratch
COPY --chown=root:root rootfs/ /
ENTRYPOINT ["/usr/sbin/init"]
