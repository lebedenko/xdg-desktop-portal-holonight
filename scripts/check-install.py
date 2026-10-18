#!/usr/bin/env python3
"""Stage payload and exercise real D-Bus activation on a bus with no desktop services."""
import os
from pathlib import Path
import subprocess
import shlex
import sys
import tempfile


def main():
    build = Path(sys.argv[1]).resolve()
    tests = Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix="portal-install-") as directory:
        stage = Path(directory)
        env = os.environ.copy()
        env["DESTDIR"] = str(stage)
        subprocess.run(["cmake", "--install", str(build)], env=env, check=True)
        services = list(stage.rglob("org.freedesktop.impl.portal.desktop.holonight.service"))
        descriptors = list(stage.rglob("holonight.portal"))
        assert len(services) == len(descriptors) == 1, "missing or duplicate activation payload"
        service = services[0]
        descriptor = descriptors[0]
        command = next(line.removeprefix("Exec=") for line in service.read_text().splitlines() if line.startswith("Exec="))
        executable = stage / shlex.split(command)[0].lstrip("/")
        assert executable.is_file(), "missing backend executable"
        assert "Interfaces=org.freedesktop.impl.portal.FileChooser;org.freedesktop.impl.portal.Settings;" in descriptor.read_text()
        assert "Name=org.freedesktop.impl.portal.desktop.holonight" in service.read_text()
        # Only the test fixture rewrites /usr to its staged path; installed payload stays relocatable by DESTDIR.
        lines = service.read_text().splitlines()
        lines = [f"Exec={shlex.quote(str(executable))}" if line.startswith("Exec=") else line for line in lines]
        service.write_text("\n".join(lines) + "\n")
        config = stage / "bus.conf"
        config.write_text(f"""<busconfig>
<type>session</type><listen>unix:tmpdir=/tmp</listen><auth>EXTERNAL</auth>
<servicedir>{service.parent}</servicedir>
<policy context="default"><allow own="*"/><allow send_destination="*"/><allow eavesdrop="true"/></policy>
</busconfig>""")
        env.pop("DESTDIR", None)
        env["PORTAL_ACTIVATION_TEST"] = "1"
        # Each order gets a new broker-less session and no shell service is visible.
        for settings_first in (False, True):
            env.pop("PORTAL_SETTINGS_FIRST", None)
            if settings_first:
                env["PORTAL_SETTINGS_FIRST"] = "1"
            subprocess.run(["dbus-run-session", f"--config-file={config}", "--", str(tests), "activationRuntime"],
                           env=env, check=True, timeout=20)


if __name__ == "__main__":
    main()
