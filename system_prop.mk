# Dalvik heap
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=192m \
    dalvik.vm.heapsize=512m

# Enable Android's BPF networking path with the backported kernel support.
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.kernel.ebpf.supported=1
