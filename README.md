<img src="https://raw.githubusercontent.com/pablogsal/SkeletonKey/main/images/logo.png" align="right" width="10%"/>


# Skeletonkey 🔓

> A fast and efficient lock trace analysis tool. 

Skeletonkey intercepts pthread locking operations to analyze contention, timing,
and usage patterns in your multi-threaded applications. 

## Features

- Low-overhead tracing of all pthread mutex, rwlock and condition variable operations
- Efficient binary trace format with varint encoding
- Detailed contention analysis
- Stack trace capture for lock operations
- Automatic busy/contention detection

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Tracing

To trace your application:

```bash
LD_PRELOAD=/path/to/libskeletonkey.so ./your_application
```

This will generate a trace file at `/tmp/skeleton_key.bin` if ``SKELETON_KEY_OUTPUT`` is not set
or at the path specified by the environment variable.

### Analysis

To analyze the trace:

```bash
python -m venv venv
source venv/bin/activate
pip install rich
python parse.py /tmp/mutrace.bin
```

This will show a table like:

```
                                      Mutex Summary
                 ╷         ╷         ╷       ╷               ╷              ╷
  Mutex #        │ #Locked │ Changed │ Cont. │ cont.Time(ms) │ tot.Time(ms) │ avg.Time(ms)
╺━━━━━━━━━━━━━━━━┿━━━━━━━━━┿━━━━━━━━━┿━━━━━━━┿━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━╸
  0x5f5db051e0a0 │      15 │      14 │    13 │     91011.222 │    99012.487 │     6600.832
```

### Environment Variables

- `SKELETONKEY_OUTPUT` - Path to trace file (default: /tmp/skeleton_key.bin)