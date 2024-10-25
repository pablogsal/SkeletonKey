import os
import sys
import pytest
import subprocess
import tempfile
import shutil
import time
from pathlib import Path

FIGHT_C = r"""
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NUM_THREADS 5
#define NUM_ITERATIONS 3

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Structure to hold thread-specific data
typedef struct
{
    int thread_id;
} thread_data_t;

void*
worker(void* arg)
{
    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Try to get the lock
        printf("Thread %d trying to acquire lock...\n", data->thread_id);
        pthread_mutex_lock(&lock);

        // Critical section
        printf("Thread %d got the lock!\n", data->thread_id);

        // Simulate some work
        usleep(1); 

        // Release the lock
        printf("Thread %d releasing lock\n", data->thread_id);
        pthread_mutex_unlock(&lock);
    }

    return NULL;
}

int
main()
{
    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    // Initialize random seed
    srand(time(NULL));

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i + 1;

        if (pthread_create(&threads[i], NULL, worker, &thread_data[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Cleanup
    pthread_mutex_destroy(&lock);

    return 0;
}
"""

@pytest.fixture(scope="session")
def build_dir():
    """Create and return a temporary build directory."""
    temp_dir = tempfile.mkdtemp()
    yield temp_dir
    shutil.rmtree(temp_dir)

@pytest.fixture(scope="session")
def skeletonkey_lib(build_dir):
    """Build skeletonkey and return path to the library."""
    # Build skeletonkey
    subprocess.run(["cmake", "-B", build_dir, "-S", "."], check=True)
    subprocess.run(["make", "-C", build_dir], check=True)
    
    lib_path = Path(build_dir) / "libskeleton_key.so"
    assert lib_path.exists(), "Library was not built successfully"
    return lib_path

@pytest.fixture(scope="session")
def fight_binary(build_dir):
    """Compile the fight.c example and return its path."""
    # Write the test program
    src_path = Path(build_dir) / "fight.c"
    src_path.write_text(FIGHT_C)
    
    # Compile it
    bin_path = Path(build_dir) / "fight"
    subprocess.run([
        "gcc", "-o", str(bin_path), str(src_path), 
        "-pthread", "-g", "-O0"
    ], check=True)
    
    assert bin_path.exists(), "Test binary was not built successfully"
    return bin_path

def test_basic_tracing(skeletonkey_lib, fight_binary):
    """Test that we can trace the fight program and generate meaningful output."""
    trace_file = "/tmp/skeleton_key.bin"
    
    # Remove any existing trace file
    if os.path.exists(trace_file):
        os.unlink(trace_file)
    
    # Run the test program under skeletonkey
    env = os.environ.copy()
    env["LD_PRELOAD"] = str(skeletonkey_lib)
    
    start_time = time.time()
    result = subprocess.run(
        [str(fight_binary)],
        env=env,
        capture_output=False,
        text=True
    )
    duration = time.time() - start_time
    
    # Check the program ran successfully
    assert result.returncode == 0, f"Program failed: {result.stderr}"
    
    # Check trace file was created
    assert os.path.exists(trace_file), "No trace file was generated"
    assert os.path.getsize(trace_file) > 0, "Trace file is empty"
    
    # Basic performance check
    assert duration < 10.0, f"Program took too long ({duration}s)"
    
    # Run the analyzer
    analyzer = Path(__file__).parent.parent / "parse.py"
    result = subprocess.run(
        [sys.executable, str(analyzer), trace_file],
        capture_output=True,
        text=True
    )
    
    assert result.returncode == 0, "Analyzer failed"
    output = result.stdout
    
    # Verify we see expected patterns in the analysis
    assert "#Locked" in output
    
    # Should see some contention
    contention_lines = [l for l in output.splitlines() 
                       if l.strip() and not l.startswith((" ", "\t"))]
    contention_found = False
    for line in contention_lines:
        if "0.000" not in line:  # Look for non-zero contention times
            contention_found = True
            break
    
    assert contention_found, "No contention detected in test program"
