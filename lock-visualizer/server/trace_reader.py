import sys
import struct
from collections import defaultdict
from enum import IntEnum
from dataclasses import dataclass
from typing import List, BinaryIO, Optional
from rich.console import Console
from rich.table import Table
from rich import box

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
    ptr1: int  # Mutex/RWLock pointer
    ptr2: int  # Secondary pointer (unused for mutexes)
    result: int
    duration_ns: int
    stack_depth: int

class LockStats:
    def __init__(self):
        self.locked_count = 0
        self.changes = 0
        self.contentions = 0
        self.contention_time_ms = 0
        self.total_time_ms = 0
        self.is_mutex = True
        self.last_owner = None

    @property
    def avg_time_ms(self):
        if self.locked_count == 0:
            return 0
        return self.total_time_ms / self.locked_count

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
        # Read event fields using varint encoding
        timestamp = read_varint(f)
        tid = read_varint(f)
        event_type = EventType(ord(f.read(1)))
        ptr1 = read_varint(f)
        ptr2 = read_varint(f)
        result = read_varint(f)
        duration = read_varint(f)
        
        # Read stack trace depth and addresses
        stack_depth = read_varint(f)
        for _ in range(stack_depth):
            read_varint(f)  # Skip stack addresses for now
        
        return Event(timestamp, tid, event_type, ptr1, ptr2, result, duration, stack_depth)
    except EOFError:
        return None