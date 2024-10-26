import sys
import struct
from collections import defaultdict
from enum import IntEnum
from dataclasses import dataclass
from typing import List, BinaryIO, Optional, Dict, Set
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

class LockStats:
    def __init__(self):
        # Existing fields
        self.locked_count = 0
        self.changes = 0
        self.contentions = 0
        self.contention_time_ms = 0
        self.total_time_ms = 0
        self.is_mutex = True
        self.last_owner = None

        # New fields for enhanced analysis
        self.threads: Set[int] = set()
        self.busy_threads: Set[int] = set()
        self.stack_traces: Set[tuple] = set()
        self.current_owners: Set[int] = set()
        self.max_wait_ms = 0
        self.max_hold_ms = 0
        self.thread_stats: Dict[int, Dict] = defaultdict(lambda: {
            'acquisitions': 0,
            'contentions': 0,
            'wait_time_ms': 0
        })
        self.acquisition_times = []
        self.hold_times = []
        self.current_holds = {}  # (tid, timestamp) pairs
        self.current_owner = None  # Currently holding thread
        self.pending_locks = {}  # tid -> timestamp of lock attempt

    @property
    def avg_time_ms(self):
        if self.locked_count == 0:
            return 0
        return self.total_time_ms / self.locked_count

    def record_lock_attempt(self, event: Event):
        # Record when a thread starts trying to acquire the lock
        # We have contention if someone else owns the lock
        is_contended = self.current_owner is not None and self.current_owner != event.tid
        self.pending_locks[event.tid] = (event.timestamp, is_contended)

    def record_acquisition(self, event: Event):
        self.locked_count += 1
        self.threads.add(event.tid)
        
        # Check if this was a contended acquisition
        if event.tid in self.pending_locks:
            start_time, was_contended = self.pending_locks[event.tid]
            if was_contended:  # Only count if it was actually contended
                self.contentions += 1
                wait_time_ms = (event.timestamp - start_time) / 1_000_000
                self.contention_time_ms += wait_time_ms
                self.max_wait_ms = max(self.max_wait_ms, wait_time_ms)
                self.busy_threads.add(event.tid)
                self.acquisition_times.append(wait_time_ms)
                
                # Update thread stats
                self.thread_stats[event.tid]['contentions'] += 1
                self.thread_stats[event.tid]['wait_time_ms'] += wait_time_ms
            
            del self.pending_locks[event.tid]
        
        # Track ownership changes
        if self.current_owner is not None and self.current_owner != event.tid:
            self.changes += 1
        
        self.current_owner = event.tid
        self.thread_stats[event.tid]['acquisitions'] += 1
        self.current_holds[event.tid] = event.timestamp
        self.current_owners.add(event.tid)


    def record_release(self, event: Event):
        if event.tid in self.current_holds:
            hold_time = (event.timestamp - self.current_holds[event.tid]) / 1_000_000
            self.total_time_ms += hold_time
            self.max_hold_ms = max(self.max_hold_ms, hold_time)
            self.hold_times.append(hold_time)
            del self.current_holds[event.tid]
            self.current_owners.discard(event.tid)
            
        if self.current_owner == event.tid:
            self.current_owner = None
def print_lock_table(locks: Dict[int, LockStats]):
    console = Console()
    table = Table(
        box=box.MINIMAL_HEAVY_HEAD,
        title="Lock Analysis Summary",
        caption="M = Mutex, W = RWLock"
    )

    # Add columns
    table.add_column("Lock #", justify="right", style="cyan")
    table.add_column("Locked", justify="right")
    table.add_column("Changed", justify="right")
    table.add_column("Cont.", justify="right")
    table.add_column("cont.Time[ms]", justify="right")
    table.add_column("tot.Time[ms]", justify="right")
    table.add_column("avg.Time[ms]", justify="right")
    table.add_column("Flags", justify="left")

    # Add rows
    for i, (addr, stats) in enumerate(sorted(locks.items())):
        if stats.locked_count == 0:
            continue

        flags = (
            'M' if stats.is_mutex else 'W',
            '-',  # State
            '-',  # Use
            '-',  # Type
            '-',  # Protocol
            '.'   # Kind
        )

        table.add_row(
            f"{i}",
            f"{stats.locked_count}",
            f"{stats.changes}",
            f"{stats.contentions}",
            f"{stats.contention_time_ms:.3f}",
            f"{stats.total_time_ms:.3f}",
            f"{stats.avg_time_ms:.3f}",
            ''.join(flags)
        )

    console.print(table)
    
def print_detailed_analysis(locks: Dict[int, LockStats]):
    console = Console()
    console.print("\n[bold]Detailed Lock Analysis[/bold]\n")

    for i, (addr, stats) in enumerate(sorted(locks.items())):
        if stats.locked_count == 0:
            continue

        console.print(f"[cyan]Lock #{i} (0x{addr:x}):[/cyan]")
        console.print(f"  Type: {'Mutex' if stats.is_mutex else 'RWLock'}")
        console.print(f"  Total acquisitions: {stats.locked_count}")
        console.print(f"  Unique threads: {len(stats.threads)}")
        
        if stats.contentions > 0:
            contention_pct = (stats.contentions * 100) / stats.locked_count
            console.print(f"  Contentions: {stats.contentions} ({contention_pct:.1f}%)")
            console.print("  Wait times:")
            console.print(f"    Total: {stats.contention_time_ms:.3f}ms")
            console.print(f"    Average: {stats.contention_time_ms/stats.contentions:.3f}ms")
            console.print(f"    Maximum: {stats.max_wait_ms:.3f}ms")

            console.print("\n  Thread contention analysis:")
            for tid, tstats in sorted(stats.thread_stats.items()):
                if tstats['contentions'] > 0:
                    avg_wait = tstats['wait_time_ms'] / tstats['contentions']
                    console.print(f"    Thread {tid}:")
                    console.print(f"      Acquisitions: {tstats['acquisitions']}")
                    console.print(f"      Contentions: {tstats['contentions']}")
                    console.print(f"      Average wait: {avg_wait:.3f}ms")

        console.print(f"\n  Hold times:")
        console.print(f"    Total: {stats.total_time_ms:.3f}ms")
        console.print(f"    Average: {stats.avg_time_ms:.3f}ms")
        console.print(f"    Maximum: {stats.max_hold_ms:.3f}ms")
        console.print("")

class LockOrderTracker:
    def __init__(self):
        self.thread_lock_orders = defaultdict(list)  # tid -> list of locks held
        self.potential_deadlocks = set()
        self.lock_order_edges = defaultdict(set)  # (lock1, lock2) -> set of threads

    def record_acquisition(self, tid: int, lock_addr: int):
        # Check current locks held by this thread for ordering violations
        held_locks = self.thread_lock_orders[tid]
        
        # Look for potential deadlocks
        for other_tid, other_locks in self.thread_lock_orders.items():
            if other_tid != tid and other_locks:
                for held_lock in other_locks:
                    # If another thread holds locks in different order
                    if held_lock in self.lock_order_edges and lock_addr in self.lock_order_edges[held_lock]:
                        self.potential_deadlocks.add(frozenset([held_lock, lock_addr]))

        # Record the lock order for this thread
        if held_locks:
            self.lock_order_edges[held_locks[-1]].add(lock_addr)
            
        held_locks.append(lock_addr)

    def record_release(self, tid: int, lock_addr: int):
        try:
            self.thread_lock_orders[tid].remove(lock_addr)
        except ValueError:
            pass  # Lock might have been acquired before tracing started

class ConvoyDetector:
    def __init__(self):
        self.current_waiters = defaultdict(set)  # lock -> set of waiting threads
        self.convoys = []
        self.CONVOY_THRESHOLD = 3  # Number of waiters that constitutes a convoy

    def record_attempt(self, tid: int, lock_addr: int, timestamp: int):
        self.current_waiters[lock_addr].add((tid, timestamp))

    def record_acquisition(self, tid: int, lock_addr: int, timestamp: int, duration_ns: int):
        waiters = self.current_waiters[lock_addr]
        if len(waiters) >= self.CONVOY_THRESHOLD:
            self.convoys.append({
                'lock': lock_addr,
                'waiters': len(waiters),
                'timestamp': timestamp,
                'duration': duration_ns,
                'waiting_threads': [w[0] for w in waiters]
            })
        # Remove this thread from waiters
        waiters = {w for w in waiters if w[0] != tid}
        self.current_waiters[lock_addr] = waiters

class StarvationDetector:
    def __init__(self):
        self.thread_stats = defaultdict(lambda: defaultdict(lambda: {
            'attempts': 0,
            'acquisitions': 0,
            'total_wait_ns': 0,
            'last_acquisition': 0,
            'max_wait_between_acquisitions_ns': 0
        }))

    def record_attempt(self, tid: int, lock_addr: int, timestamp: int):
        self.thread_stats[lock_addr][tid]['attempts'] += 1

    def record_acquisition(self, tid: int, lock_addr: int, timestamp: int, duration_ns: int):
        stats = self.thread_stats[lock_addr][tid]
        stats['acquisitions'] += 1
        stats['total_wait_ns'] += duration_ns
        
        if stats['last_acquisition'] > 0:
            wait_time = timestamp - stats['last_acquisition']
            stats['max_wait_between_acquisitions_ns'] = max(
                stats['max_wait_between_acquisitions_ns'],
                wait_time
            )
        stats['last_acquisition'] = timestamp

    def get_starved_threads(self, threshold_ms: float = 1000.0):
        """Identify threads that wait much longer than others for the same lock."""
        starved = []
        
        for lock_addr, thread_data in self.thread_stats.items():
            if len(thread_data) < 2:  # Need multiple threads for starvation
                continue
                
            # Calculate average wait times per thread
            avg_waits = {
                tid: stats['total_wait_ns'] / stats['attempts']
                for tid, stats in thread_data.items()
                if stats['attempts'] > 0
            }
            
            if not avg_waits:
                continue
                
            median_wait = sorted(avg_waits.values())[len(avg_waits)//2]
            
            for tid, avg_wait in avg_waits.items():
                if avg_wait > median_wait * 5:  # Thread waits 5x longer than median
                    starved.append({
                        'lock': lock_addr,
                        'thread': tid,
                        'avg_wait_ms': avg_wait / 1_000_000,
                        'median_wait_ms': median_wait / 1_000_000,
                        'attempts': thread_data[tid]['attempts'],
                        'acquisitions': thread_data[tid]['acquisitions']
                    })
        
        return starved

def analyze_locks(events: List[Event]) -> Dict[int, LockStats]:
    locks = defaultdict(LockStats)
    order_tracker = LockOrderTracker()
    convoy_detector = ConvoyDetector()
    starvation_detector = StarvationDetector()
    
    for event in events:
        lock = locks[event.ptr1]
        
        if event.type == EventType.MutexLock:
            # Record the attempt starting time
            lock.record_lock_attempt(event)
            convoy_detector.record_attempt(event.tid, event.ptr1, event.timestamp)
            starvation_detector.record_attempt(event.tid, event.ptr1, event.timestamp)
            
        elif event.type == EventType.MutexLockDone:
            assert event.tid in lock.pending_locks
            lock.record_acquisition(event)
            order_tracker.record_acquisition(event.tid, event.ptr1)
            
            # Only record convoy if there was actual waiting
            if event.tid in lock.pending_locks:
                wait_time = event.duration_ns
                if wait_time > 0:
                    convoy_detector.record_acquisition(
                        event.tid, event.ptr1, event.timestamp, wait_time
                    )
                    starvation_detector.record_acquisition(
                        event.tid, event.ptr1, event.timestamp, wait_time
                    )
            
        elif event.type == EventType.MutexUnlock:
            lock.record_release(event)
            order_tracker.record_release(event.tid, event.ptr1)

    return locks, order_tracker, convoy_detector, starvation_detector


def print_risk_analysis(order_tracker: LockOrderTracker, 
                       convoy_detector: ConvoyDetector,
                       starvation_detector: StarvationDetector):
    console = Console()
    console.print("\n[bold red]Risk Analysis[/bold red]\n")

    # Deadlock risks
    if order_tracker.potential_deadlocks:
        console.print("[bold yellow]Potential Deadlock Patterns:[/bold yellow]")
        for locks in order_tracker.potential_deadlocks:
            lock1, lock2 = locks
            console.print(f"  Lock pair: 0x{lock1:x} ←→ 0x{lock2:x}")
            console.print("  Different threads acquire these locks in opposite orders!")
    else:
        console.print("[green]No deadlock patterns detected[/green]")

    # Lock convoys
    if convoy_detector.convoys:
        console.print("\n[bold yellow]Lock Convoys Detected:[/bold yellow]")
        for convoy in convoy_detector.convoys:
            console.print(f"  Lock 0x{convoy['lock']:x}:")
            console.print(f"    {convoy['waiters']} threads queued")
            console.print(f"    Wait duration: {convoy['duration']/1e6:.3f}ms")
            console.print(f"    Waiting threads: {', '.join(str(t) for t in convoy['waiting_threads'])}")
    else:
        console.print("\n[green]No lock convoys detected[/green]")

    # Thread starvation
    starved = starvation_detector.get_starved_threads()
    if starved:
        console.print("\n[bold yellow]Thread Starvation Detected:[/bold yellow]")
        for case in starved:
            console.print(f"  Lock 0x{case['lock']:x}:")
            console.print(f"    Thread {case['thread']} potentially starving:")
            console.print(f"      Average wait: {case['avg_wait_ms']:.3f}ms")
            console.print(f"      Median wait: {case['median_wait_ms']:.3f}ms")
            console.print(f"      Success rate: {case['acquisitions']}/{case['attempts']}")
    else:
        console.print("\n[green]No thread starvation detected[/green]")

# Modify main() to include risk analysis
def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <trace file>", file=sys.stderr)
        sys.exit(1)

    events = []
    with open(sys.argv[1], "rb") as f:
        while True:
            event = read_event(f)
            if event is None:
                break
            events.append(event)

    locks, order_tracker, convoy_detector, starvation_detector = analyze_locks(events)
    print_lock_table(locks)
    print_detailed_analysis(locks)
    print_risk_analysis(order_tracker, convoy_detector, starvation_detector)

if __name__ == "__main__":
    main()