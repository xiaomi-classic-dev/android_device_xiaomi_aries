Xiaomi MI 2 / MI 2S (aries) recovery repartition package.

Target layout:
- system:   2048 MiB, sectors 655360-4849663
- cache:     384 MiB, sectors 4849664-5636095
- userdata: remainder, sectors 5636096-last usable GPT sector

This package wipes system, cache, userdata, and internal storage.
It only runs when the current partition table exactly matches the stock
layout observed on aries:
- system:   sectors 655360-2752511
- cache:    sectors 2752512-3538943
- userdata: starts at sector 3538944 and extends to the last usable GPT sector

This supports 16G and 32G devices if their stock aries partition layout before
userdata matches the values above. The extra 1024 MiB needed by system is taken
from userdata; cache remains 384 MiB.

Install from recovery or sideload with:
  adb sideload aries_repartition_system2048_cache384.zip

After it finishes, the device reboots back to recovery. Flash a ROM built
with BOARD_SYSTEMIMAGE_PARTITION_SIZE := 2147483648.
