#!/sbin/sh
# The changed Data boundary has no filesystem until the user confirms finish.
# Preflight before TWRP's fstab processing, not from postrecoveryboot.sh.
if /sbin/aries-repartition probe && [ "$(getprop aries.resize.ready)" = "1" ]; then
    cp /etc/aries-recovery.fstab /etc/recovery.fstab
else
    # Keep unformatted or unverified partitions out of TWRP's automatic
    # media setup, filesystem probes and size queries. The GPT tool still
    # validates the real block devices before every repair/finish action.
    cp /etc/aries-recovery-pending.fstab /etc/recovery.fstab
fi
exec /sbin/recovery "$@"
