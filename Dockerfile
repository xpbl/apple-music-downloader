# this docker base image can fit on a high density floppy!
FROM busybox:glibc
# --------------------------------------------------------
COPY rootfs/ /
CMD ["/amwrapper"]
