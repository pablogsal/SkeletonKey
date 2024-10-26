from flask import Flask, jsonify, send_from_directory, request
import sys
from collections import defaultdict
from typing import Dict, List, Set
from .trace_reader import read_event, EventType

app = Flask(__name__, static_folder='../static', static_url_path='')

class TimelineData:
    def __init__(self):
        self.events_by_lock: Dict[int, List[dict]] = defaultdict(list)
        self.threads: Set[int] = set()
        
    def process_event(self, event):
        print(event)
        self.threads.add(event.tid)

        if event.type in [EventType.MutexLock, EventType.MutexTryLock, EventType.MutexTimedLock]:
            self.events_by_lock[event.ptr1].append({
                'tid': event.tid,
                'type': 'wait',
                'timestamp': event.timestamp
            })
            
        elif event.type in [EventType.MutexLockDone, EventType.MutexTryLockDone, EventType.MutexTimedLockDone]:
            wait_event = next(
                (e for e in reversed(self.events_by_lock[event.ptr1])
                 if e['tid'] == event.tid and e['type'] == 'wait' and 'duration' not in e),
                None
            )
            
            if wait_event:
                # Set wait duration
                wait_event['duration'] = event.timestamp - wait_event['timestamp']
                
                # Start held period
                self.events_by_lock[event.ptr1].append({
                    'tid': event.tid,
                    'type': 'held',
                    'timestamp': event.timestamp
                })
                
        elif event.type == EventType.MutexUnlock:
            # Find the matching held event
            held_event = next(
                (e for e in reversed(self.events_by_lock[event.ptr1])
                 if e['tid'] == event.tid and e['type'] == 'held' and 'duration' not in e),
                None
            )
            
            if held_event:
                # Set held duration from LockDone to Unlock
                held_event['duration'] = event.timestamp - held_event['timestamp']

@app.route('/api/locks')
def get_locks():
    return jsonify({
        'locks': list(timeline_data.events_by_lock.keys()),
        'threads': list(timeline_data.threads)
    })

@app.route('/api/timeline/<int:lock_addr>')
def get_timeline(lock_addr):
    events = timeline_data.events_by_lock.get(lock_addr, [])
    return jsonify({
        'events': events,
        'threads': list(timeline_data.threads)
    })

@app.route('/api/multi_timeline', methods=['POST'])
def get_multi_timeline():
    # Expect a list of lock addresses in the request
    lock_addresses = request.json['locks']
    
    # Get all events for requested locks
    events = []
    threads = set()
    
    for lock_addr in lock_addresses:
        lock_events = timeline_data.events_by_lock.get(lock_addr, [])
        # Only include 'held' events for the overlap view
        held_events = [
            {**e, 'lock_addr': lock_addr} 
            for e in lock_events 
            if e['type'] == 'held'
        ]
        events.extend(held_events)
        threads.update(e['tid'] for e in held_events)
        
    return jsonify({
        'events': events,
        'threads': list(threads)
    })

@app.route('/')
def root():
    return app.send_static_file('index.html')

timeline_data = TimelineData()

def load_trace_file(filename):
    with open(filename, 'rb') as f:
        while True:
            event = read_event(f)
            if event is None:
                break
            timeline_data.process_event(event)

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <trace file>", file=sys.stderr)
        sys.exit(1)
        
    load_trace_file(sys.argv[1])
    app.run(debug=True)

if __name__ == '__main__':
    main()