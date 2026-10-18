# Dalvik heap
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=192m \
    dalvik.vm.heapsize=512m

# Enable Android's BPF networking path with the backported kernel support.
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.kernel.ebpf.supported=1

# Explicit ARM32 opt-in; keep the actual kernel release for ELF selection.
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += ro.bpf.allow_32bit=true

# Share the platform HIDL registry for passthrough HALs and expose the
# platform AudioSystem client required by the legacy Qualcomm RIL.
PRODUCT_VENDOR_PROPERTIES += ro.vendor.aries.legacy_linker=true
