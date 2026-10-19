#!/usr/bin/env python3
"""Measure process/thread CPU deltas with the macOS SDK's read-only libproc API.

CPU percentages use one fully occupied logical core as 100%. Task totals use
Mach absolute time, whereas proc_threadinfo exposes nanoseconds. Convert only
the former using mach_timebase_info; do not combine those raw clock units.
No application preferences, credentials or business state are read or written.
"""

import argparse
import ctypes
import json
from pathlib import Path
import sys
import time


class TaskInfo(ctypes.Structure):
    _fields_ = [
        (name, ctypes.c_uint64)
        for name in (
            "virtual_size",
            "resident_size",
            "user",
            "system",
            "threads_user",
            "threads_system",
        )
    ] + [
        (name, ctypes.c_int32)
        for name in (
            "policy",
            "faults",
            "pageins",
            "cow_faults",
            "messages_sent",
            "messages_received",
            "syscalls_mach",
            "syscalls_unix",
            "context_switches",
            "thread_count",
            "running",
            "priority",
        )
    ]


class ThreadInfo(ctypes.Structure):
    _fields_ = (
        [("user", ctypes.c_uint64), ("system", ctypes.c_uint64)]
        + [
            (name, ctypes.c_int32)
            for name in (
                "cpu_usage",
                "policy",
                "run_state",
                "flags",
                "sleep_time",
                "current_priority",
                "priority",
                "max_priority",
            )
        ]
        + [("name", ctypes.c_char * 64)]
    )


class Timebase(ctypes.Structure):
    _fields_ = [("numerator", ctypes.c_uint32), ("denominator", ctypes.c_uint32)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pid", type=int)
    parser.add_argument("--seconds", type=float, default=10)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if sys.platform != "darwin" or not 1 <= args.seconds <= 60 or args.pid < 1:
        parser.error("Requires macOS, a positive PID and 1–60 seconds")
    library = ctypes.CDLL("/usr/lib/libproc.dylib", use_errno=True)
    library.proc_pidinfo.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_uint64,
        ctypes.c_void_p,
        ctypes.c_int,
    ]
    library.proc_pidinfo.restype = ctypes.c_int
    system = ctypes.CDLL("/usr/lib/libSystem.B.dylib")
    system.mach_timebase_info.argtypes = [ctypes.POINTER(Timebase)]
    base = Timebase()
    if system.mach_timebase_info(ctypes.byref(base)) or not base.denominator:
        raise RuntimeError("Cannot obtain Mach timebase")
    ratio = base.numerator / base.denominator

    def snapshot():
        task = TaskInfo()
        if library.proc_pidinfo(
            args.pid, 4, 0, ctypes.byref(task), ctypes.sizeof(task)
        ) != ctypes.sizeof(task):
            raise OSError(ctypes.get_errno(), "Cannot read proc_taskinfo")
        ids = (ctypes.c_uint64 * 2048)()
        size = library.proc_pidinfo(args.pid, 6, 0, ids, ctypes.sizeof(ids))
        if size <= 0 or size >= ctypes.sizeof(ids):
            raise RuntimeError("Cannot obtain a complete thread list")
        threads = {}
        for tid in ids[: size // 8]:
            thread = ThreadInfo()
            if library.proc_pidinfo(
                args.pid, 5, tid, ctypes.byref(thread), ctypes.sizeof(thread)
            ) == ctypes.sizeof(thread):
                threads[tid] = {
                    "name": thread.name.decode(errors="replace"),
                    "ns": thread.user + thread.system,
                }
        return time.monotonic(), (task.user + task.system) * ratio, threads

    start = snapshot()
    time.sleep(args.seconds)
    end = snapshot()
    wall = end[0] - start[0]
    rows = []
    for tid, current in end[2].items():
        if tid in start[2]:
            rows.append(
                {
                    "thread": tid,
                    "name": current["name"],
                    "cpu_percent": round(
                        (current["ns"] - start[2][tid]["ns"]) / 1e9 / wall * 100, 2
                    ),
                }
            )
    result = {
        "pid": args.pid,
        "wall_seconds": round(wall, 3),
        "cpu_percent": round((end[1] - start[1]) / 1e9 / wall * 100, 2),
        "threads": sorted(rows, key=lambda row: -row["cpu_percent"]),
    }
    data = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(data)
    else:
        print(data, end="")


if __name__ == "__main__":
    main()
