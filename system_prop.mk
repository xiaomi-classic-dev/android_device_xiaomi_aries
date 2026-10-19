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

# Keep the real kernel LMK; Android 17 still sends LMK_START_MONITORING at boot.
# Zero thresholds skip PSI trigger creation on this kernel, as on SFO.
PRODUCT_SYSTEM_PROPERTIES += \
    ro.lmk.use_new_strategy=true \
    ro.lmk.psi_partial_stall_ms=0 \
    ro.lmk.psi_complete_stall_ms=0
