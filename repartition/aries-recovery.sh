#!/sbin/sh
# Complete the transaction authorized by the user's persisted exact "yes".
# This runs before TWRP can mount the changed System/Cache/Data boundaries.
# finish checks journal CRC/CID/confirmation, both GPT copies, protected entries
# and freshly booted kernel geometry before it invokes any formatter.
/sbin/aries-repartition probe
if [ "$(getprop aries.resize.phase)" = "finish" ]; then
    if /sbin/aries-repartition finish yes > /tmp/aries-repartition-resume.log 2>&1; then
        /sbin/aries-repartition probe
    else
        error="$(getprop aries.resize.error)"
        /sbin/aries-repartition probe
        setprop aries.resize.error "$error"
    fi
fi
if [ "$(getprop aries.resize.ready)" = "1" ]; then
    cp /etc/aries-recovery.fstab /etc/recovery.fstab
else
    # Keep unformatted or unverified partitions out of TWRP's automatic
    # media setup, filesystem probes and size queries. The GPT tool still
    # validates the real block devices before every repair/finish action.
    cp /etc/aries-recovery-pending.fstab /etc/recovery.fstab
fi
exec /sbin/recovery "$@"
