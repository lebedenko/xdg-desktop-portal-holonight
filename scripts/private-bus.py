#!/usr/bin/env python3
"""Run a test/probe on a private non-activating bus, preserving the real session environment."""
import argparse
from pathlib import Path
import selectors
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('An acceptance executable is required')
    with subprocess.Popen(['dbus-daemon', '--nofork', '--print-address=1',
                           '--config-file=' + str(args.config)], stdout=subprocess.PIPE, text=True) as bus:
        try:
            with selectors.DefaultSelector() as selector:
                selector.register(bus.stdout, selectors.EVENT_READ)
                if not selector.select(5):
                    raise RuntimeError('Private D-Bus did not become ready')
            address = bus.stdout.readline().strip()
            if not address.startswith('unix:'):
                raise RuntimeError('Private D-Bus returned no Unix address')
            # Never assign DBUS_SESSION_BUS_ADDRESS: Images uses the real user manager.
            return subprocess.call([*command, '--portal-bus', address])
        finally:
            bus.terminate()
            try:
                bus.wait(timeout=5)
            except subprocess.TimeoutExpired:
                bus.kill()
                bus.wait()


if __name__ == '__main__':
    raise SystemExit(main())
