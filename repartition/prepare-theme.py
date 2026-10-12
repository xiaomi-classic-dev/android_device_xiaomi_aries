#!/usr/bin/env python
# SPDX-License-Identifier: Apache-2.0
from __future__ import print_function
import hashlib
import os
import re
import shutil
import sys
import xml.etree.ElementTree as ET

source, root, local = sys.argv[1:]
capacity = open(os.path.join(local, 'capacity.h')).read()
minimum = re.search(r'ARIES_SYSTEM_MIN_MIB\s+(\d+)', capacity).group(1)
maximum = re.search(r'ARIES_SYSTEM_MAX_MIB\s+(\d+)', capacity).group(1)
def replace(node):
    for n in node.iter():
        if n.text:
            n.text = n.text.replace('@MIN_MIB@', minimum).replace('@MAX_MIB@', maximum)
        for k, v in list(n.attrib.items()):
            n.set(k, v.replace('@MIN_MIB@', minimum).replace('@MAX_MIB@', maximum))
def save(tree, path):
    tree.write(path, encoding='utf-8', xml_declaration=True)
portrait = ET.parse(source)
pages = portrait.getroot().find('pages')
advanced = next(p for p in pages if p.get('name') == 'advanced')
listing = next(n for n in advanced if n.tag == 'listbox' and n.get('style') == 'advanced_listbox')
extension = ET.parse(os.path.join(local, 'pages.xml')).getroot()
replace(extension)
items = listing.findall('listitem')
position = list(listing).index(items[1]) if len(items) > 1 else len(list(listing))
listing.insert(position, extension.find('listitem'))
ET.SubElement(listing, 'data', name='aries_advanced_selection', default='0')
for index, item in enumerate(listing.findall('listitem')):
    item.text = str(index)
reset = ET.SubElement(advanced, 'action')
ET.SubElement(reset, 'action', function='set').text = 'aries_advanced_selection=0'
for page in extension.find('pages'):
    pages.append(page)
# apply uses a size argument; finish/repair/restore do not.
work = next(p for p in pages if p.get('name') == 'aries_work')
base = next(n for n in work if n.tag == 'action' and n.find('actions') is not None)
import copy
apply = copy.deepcopy(base)
ET.SubElement(apply, 'condition', var1='aries_mode', var2='apply')
for n in apply.find('actions'):
    if n.get('function') == 'cmd':
        n.text = '/sbin/aries-repartition apply %aries_system_mib% %aries_confirm%'
ET.SubElement(base, 'condition', var1='aries_mode', op='!=', var2='apply')
work.insert(list(work).index(base), apply)
save(portrait, os.path.join(root, 'twres', 'portrait.xml'))
ui_path = os.path.join(root, 'twres', 'ui.xml')
ui = ET.parse(ui_path)
variables = ui.getroot().find('variables')
for name, value in [('aries_system_mib', minimum), ('aries_confirm', ''),
                    ('aries_ready', '0'), ('aries_installed', '0'), ('aries_finish', '0'), ('aries_repair', '0'),
                    ('aries_probing', '0'),
                    ('aries_restore', '0'), ('aries_mode', 'apply'), ('aries_run', '0')]:
    ET.SubElement(variables, 'variable', name=name, value=value)
save(ui, ui_path)
for lang in ('en', 'zh_CN'):
    path = os.path.join(root, 'twres', 'languages', lang + '.xml')
    tree = ET.parse(path)
    resources = tree.getroot().find('resources')
    for string in ET.parse(os.path.join(local, lang + '.xml')).getroot():
        replace(string)
        resources.append(string)
    save(tree, path)
shutil.copyfile(os.path.join(local, 'postrecoveryboot.sh'), os.path.join(root, 'sbin', 'postrecoveryboot.sh'))
os.chmod(os.path.join(root, 'sbin', 'postrecoveryboot.sh'), 0o755)
shutil.copyfile(os.path.join(local, 'aries-recovery.sh'), os.path.join(root, 'sbin', 'aries-recovery.sh'))
os.chmod(os.path.join(root, 'sbin', 'aries-recovery.sh'), 0o755)
# While GPT migration is pending, the new ext4 boundaries are intentionally
# unformatted. Defer TWRP's automatic mounts/probes until a fresh Recovery boot
# sees the completed journal; the repair tool accesses the real devices itself.
fstab = open(os.path.join(local, '..', 'recovery.fstab')).read()
with open(os.path.join(root, 'etc', 'aries-recovery.fstab'), 'w') as f:
    f.write(fstab)
pending = []
for line in fstab.splitlines(True):
    fields = line.split()
    if fields and fields[0] in ('/system', '/data', '/cache'):
        continue
    pending.append(line)
with open(os.path.join(root, 'etc', 'aries-recovery-pending.fstab'), 'w') as f:
    f.write(''.join(pending))
service_path = os.path.join(root, 'init.recovery.service.rc')
service = open(service_path).read()
service = service.replace('service recovery /sbin/recovery', 'service recovery /sbin/aries-recovery.sh')
with open(service_path, 'w') as f:
    f.write(service)
with open(os.path.join(root, 'etc', 'aries-repartition-ready'), 'w') as f:
    f.write('aries-gpt-v1\n')
# Keep the generated ramdisk checksum inventory accurate after device hooks.
path = os.path.join(root, 'ramdisk-files.sha256sum')
if os.path.exists(path):
    names = [line.rstrip('\n').split('  ', 1)[1] for line in open(path) if '  ' in line]
    for name in ('sbin/postrecoveryboot.sh', 'etc/aries-repartition-ready',
                 'sbin/aries-recovery.sh', 'etc/aries-recovery.fstab', 'etc/aries-recovery-pending.fstab'):
        if name not in names:
            names.append(name)
    with open(path, 'w') as f:
        for name in names:
            with open(os.path.join(root, name), 'rb') as data:
                f.write(hashlib.sha256(data.read()).hexdigest() + '  ' + name + '\n')
print('Aries physical partition UI added: %s..%s MiB' % (minimum, maximum))
