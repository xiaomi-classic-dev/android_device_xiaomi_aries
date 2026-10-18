#!/usr/bin/env python3
"""Materialize only the BoringSSL rc aliases rejected by init's O_NOFOLLOW."""
import os
from pathlib import Path
import sys
import tempfile

product = Path(sys.argv[1])
marker = Path(sys.argv[2])
for directory, prefix in [
    (product / "system/etc/init/hw", "init.boringssl"),
    (product / "system/vendor/etc", "boringssl_self_test"),
]:
    # Aries uses a vendor directory inside its system partition.
    source = directory / (prefix + ".zygote64_32.rc")
    if source.is_symlink() or not source.is_file():
        raise RuntimeError("Expected regular platform config: " + str(source))
    contents = source.read_bytes()
    for variant in ["zygote32", "no_zygote"]:
        destination = directory / (prefix + "." + variant + ".rc")
        if destination.is_symlink():
            if os.readlink(destination) != source.name:
                raise RuntimeError("Unexpected BoringSSL link: " + str(destination))
        elif destination.is_file():
            if destination.read_bytes() == contents:
                os.chmod(destination, 0o644)
                continue
        else:
            raise RuntimeError("Missing platform import: " + str(destination))
        fd, temporary = tempfile.mkstemp(prefix=".aries-boringssl-", dir=directory)
        try:
            with os.fdopen(fd, "wb") as output:
                output.write(contents)
                os.fchmod(output.fileno(), 0o644)
            os.replace(temporary, destination)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
# Keep the module newer than its alias dependencies for incremental builds.
os.utime(marker, None)
