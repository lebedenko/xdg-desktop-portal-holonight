#!/usr/bin/env python3
"""Verify the complete production install payload, excluding test/development tools."""
from pathlib import Path
import shlex
import sys
root = Path(sys.argv[1])
expected = {
    'usr/libexec/xdg-desktop-portal-holonight',
    'usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.holonight.service',
    'usr/share/xdg-desktop-portal/portals/holonight.portal',
    'usr/share/licenses/xdg-desktop-portal-holonight/LICENSE',
    'usr/share/licenses/xdg-desktop-portal-holonight/GPL-3.0-or-later.txt',
}
actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
if actual != expected:
    sys.exit(f'Unexpected payload: missing={expected - actual}, extra={actual - expected}')
service = root / 'usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.holonight.service'
command = next(line.removeprefix('Exec=') for line in service.read_text().splitlines() if line.startswith('Exec='))
if shlex.split(command) != ['/usr/libexec/xdg-desktop-portal-holonight']:
    sys.exit('Incorrect staged activation executable')

descriptor = root / 'usr/share/xdg-desktop-portal/portals/holonight.portal'
assert 'DBusName=org.freedesktop.impl.portal.desktop.holonight' in descriptor.read_text()
assert 'Interfaces=org.freedesktop.impl.portal.FileChooser;org.freedesktop.impl.portal.Settings;' in descriptor.read_text()
assert 'Name=org.freedesktop.impl.portal.desktop.holonight' in service.read_text()
print('Release payload: PASS (five production files)')
