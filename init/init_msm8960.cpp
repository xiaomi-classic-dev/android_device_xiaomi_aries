/*
   Copyright (c) 2013, The Linux Foundation. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <unistd.h>

#include "vendor_init.h"

#define ISMATCH(a,b)    (!strncmp(a,b,PROP_VALUE_MAX))

#define RAW_ID_PATH     "/sys/devices/system/soc/soc0/raw_id"
#define BUF_SIZE         64
static char tmp[BUF_SIZE];

static int read_file2(const char *fname, char *data, int max_size)
{
    int fd, rc;

    if (max_size < 1)
        return 0;

    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    rc = read(fd, data, max_size - 1);
    if ((rc > 0) && (rc < max_size))
        data[rc] = '\0';
    else
        data[0] = '\0';
    close(fd);

    return 1;
}

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void property_override_dual(char const system_prop[],
        char const vendor_prop[], char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
}

void vendor_load_properties()
{
    int rc;
    unsigned long raw_id = -1;

    /* get raw ID */
    rc = read_file2(RAW_ID_PATH, tmp, sizeof(tmp));
    if (rc) {
        raw_id = strtoul(tmp, NULL, 0);
    }

    /* MI 2 */
    if (raw_id==1816) {
        property_override_dual("ro.product.model", "ro.vendor.product.model", "MI 2");
    }

    /* MI 2S */
    else if(raw_id==1812) {
        property_override_dual("ro.product.model", "ro.vendor.product.model", "MI 2S");
    }

    /* Unknown */
    else {
        property_override_dual("ro.product.model", "ro.vendor.product.model", "MI 2C");
    }

    property_override_dual("ro.product.device", "ro.vendor.product.device", "aries");
    property_override_dual("ro.product.name", "ro.vendor.product.name", "aries");
    property_override("ro.build.description", "aries-userdebug 6.0.1 MMB29M 6.3.22 test-keys");
    property_override("ro.build.fingerprint", "Xiaomi/aries/aries:6.0.1/MMB29M/6.3.22:userdebug/test-keys");
}
