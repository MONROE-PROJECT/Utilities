# Watchdog Scripts

[watchdog](https://linux.die.net/man/8/watchdog) is a daemon which can perform
several types of system status checks and will reboot the system should one of
these checks fail.

One of these checks is running executables in `/etc/watchdog.d` (or other
location if configured). These tests are performed by running the executable
with the argument 'test', and in case of a failure (non-zero exit code) the same
executable is run with the argument 'repair'. Should the test fail repeatedly,
or the repair fail, the system will reboot.
