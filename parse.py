#!/usr/bin/env python3
import sys
import struct
from enum import IntEnum
from dataclasses import dataclass
from typing import List, BinaryIO, Optional
import time


class EventType(IntEnum):
    # Thread events
    ThreadCreate = 0

    # Mutex events
    MutexInit = 1
    MutexDestroy = 2
    MutexLock = 3
    MutexLockDone = 4
    MutexTryLock = 5
    MutexTryLockDone = 6
    MutexTimedLock = 7
    MutexTimedLockDone = 8
    MutexUnlock = 9

    # RWLock events
    RWLockInit = 10
    RWLockDestroy = 11
    RWLockRead = 12
    RWLockReadDone = 13
    RWLockTryRead = 14
    RWLockTryReadDone = 15
    RWLockTimedRead = 16
    RWLockTimedReadDone = 17
    RWLockWrite = 18
    RWLockWriteDone = 19
    RWLockTryWrite = 20
    RWLockTryWriteDone = 21
    RWLockTimedWrite = 22
    RWLockTimedWriteDone = 23
    RWLockUnlock = 24

    # Condition variable events
    CondInit = 25
    CondDestroy = 26
    CondSignal = 27
    CondBroadcast = 28
    CondWait = 29
    CondWaitDone = 30
    CondTimedWait = 31
    CondTimedWaitDone = 32


@dataclass
class Event:
    timestamp: int
    tid: int
    type: EventType
    ptr1: int
    ptr2: int
    result: int
    duration_ns: int
    stack: List[int]


def read_varint(f: BinaryIO) -> int:
    result = 0
    shift = 0

    while True:
        byte = f.read(1)
        if not byte:
            raise EOFError

        b = byte[0]
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            break
        shift += 7

    return result


def read_event(f: BinaryIO) -> Optional[Event]:
    try:
        timestamp = read_varint(f)
        tid = read_varint(f)
        event_type = EventType(f.read(1)[0])
        ptr1 = read_varint(f)
        ptr2 = read_varint(f)
        result = read_varint(f)
        duration = read_varint(f)

        # Read stack trace
        depth = read_varint(f)
        stack = []
        for _ in range(depth):
            addr = read_varint(f)
            stack.append(addr)

        return Event(
            timestamp=timestamp,
            tid=tid,
            type=event_type,
            ptr1=ptr1,
            ptr2=ptr2,
            result=result,
            duration_ns=duration,
            stack=stack,
        )
    except EOFError:
        return None


def format_duration(ns: int) -> str:
    if ns == 0:
        return ""
    return f"{ns/1e9:.6f}s"


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <trace file>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], "rb") as f:
        first_timestamp = None

        while True:
            event = read_event(f)
            if event is None:
                break

            if first_timestamp is None:
                first_timestamp = event.timestamp

            relative_time = (event.timestamp - first_timestamp) / 1e9

            # Print event
            print(
                f"{relative_time:10.6f} tid={event.tid:<5} {event.type.name:20} "
                f"ptr=0x{event.ptr1:x}",
                end="",
            )

            if event.ptr2:
                print(f" aux_ptr=0x{event.ptr2:x}", end="")

            if event.duration_ns:
                print(f" duration={format_duration(event.duration_ns)}", end="")

            if event.result:
                print(f" result={event.result}", end="")

            print("\nStack trace:")
            for addr in event.stack:
                print(f"  0x{addr:x}")
            print()


if __name__ == "__main__":
    main()
