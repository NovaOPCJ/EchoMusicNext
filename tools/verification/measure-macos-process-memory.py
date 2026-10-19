#!/usr/bin/env python3
"""Read CPU time, RSS, physical footprint and lifetime peak through macOS libproc.

The SDK's rusage_info_v4 reports memory in bytes; CPU Mach ticks are converted to ns.
Virtual address reservations
and mapped shared library sizes are deliberately not treated as physical usage.
This tool reads no allocation contents, account data or application preferences.
"""

import argparse
import ctypes
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import time


class UsageInfo(ctypes.Structure):
    _fields_ = [("uuid", ctypes.c_uint8 * 16)] + [
        (name, ctypes.c_uint64)
        for name in (
            "user_time",
            "system_time",
            "pkg_idle_wakeups",
            "interrupt_wakeups",
            "pageins",
            "wired_size",
            "resident_size",
            "phys_footprint",
            "start_time",
            "exit_time",
            "child_user_time",
            "child_system_time",
            "child_pkg_idle_wakeups",
            "child_interrupt_wakeups",
            "child_pageins",
            "child_elapsed_time",
            "diskio_bytesread",
            "diskio_byteswritten",
            "qos_default",
            "qos_maintenance",
            "qos_background",
            "qos_utility",
            "qos_legacy",
            "qos_user_initiated",
            "qos_user_interactive",
            "billed_system_time",
            "serviced_system_time",
            "logical_writes",
            "lifetime_max_phys_footprint",
            "instructions",
            "cycles",
            "billed_energy",
            "serviced_energy",
            "interval_max_phys_footprint",
            "runnable_time",
        )
    ]


def cpu_timebase():
    """proc_pid_rusage CPU times use Mach ticks, not nanoseconds on Apple Silicon."""

    class Timebase(ctypes.Structure):
        _fields_ = [("numer", ctypes.c_uint32), ("denom", ctypes.c_uint32)]

    library = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
    info = Timebase()
    if library.mach_timebase_info(ctypes.byref(info)) != 0 or not info.denom:
        raise RuntimeError("Cannot read Mach CPU timebase")
    return info.numer, info.denom


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--samples", type=int, default=1)
    parser.add_argument("--interval", type=float, default=1)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if (
        sys.platform != "darwin"
        or args.pid < 1
        or not 1 <= args.samples <= 30
        or not 0.1 <= args.interval <= 10
        or (args.samples - 1) * args.interval > 60
    ):
        parser.error("Requires macOS, positive PID, 1–30 samples and at most 60 seconds")
    library = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
    library.proc_pid_rusage.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
    library.proc_pid_rusage.restype = ctypes.c_int
    numer, denom = cpu_timebase()
    rows = []
    for i in range(args.samples):
        if i:
            time.sleep(args.interval)
        usage = UsageInfo()
        if library.proc_pid_rusage(args.pid, 4, ctypes.byref(usage)) != 0:
            raise OSError(ctypes.get_errno(), "Cannot read rusage_info_v4")
        if rows and usage.start_time != rows[0]["start_time"]:
            raise RuntimeError("PID was reused during sampling")
        rows.append(
            {
                "sampled_at_utc": datetime.now(timezone.utc).isoformat(),
                "start_time": usage.start_time,
                "monotonic_ns": time.monotonic_ns(),
                "user_cpu_ns": usage.user_time * numer // denom,
                "system_cpu_ns": usage.system_time * numer // denom,
                "rss_bytes": usage.resident_size,
                "physical_footprint_bytes": usage.phys_footprint,
                "lifetime_peak_footprint_bytes": usage.lifetime_max_phys_footprint,
                "wired_bytes": usage.wired_size,
            }
        )
    data = json.dumps({"pid": args.pid, "samples": rows}, indent=2) + "\n"
    if args.output:
        args.output.write_text(data)
    else:
        print(data, end="")


if __name__ == "__main__":
    main()
