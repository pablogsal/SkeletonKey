#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <fstream>
#include <mutex>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>

namespace skeleton_key {

enum class EventType : uint8_t {
    // Thread events
    ThreadCreate,

    // Mutex events
    MutexInit,
    MutexDestroy,
    MutexLock,
    MutexLockDone,
    MutexTryLock,
    MutexTryLockDone,
    MutexTimedLock,
    MutexTimedLockDone,
    MutexUnlock,

    // RWLock events
    RWLockInit,
    RWLockDestroy,
    RWLockRead,
    RWLockReadDone,
    RWLockTryRead,
    RWLockTryReadDone,
    RWLockTimedRead,
    RWLockTimedReadDone,
    RWLockWrite,
    RWLockWriteDone,
    RWLockTryWrite,
    RWLockTryWriteDone,
    RWLockTimedWrite,
    RWLockTimedWriteDone,
    RWLockUnlock,

    // Condition variable events
    CondInit,
    CondDestroy,
    CondSignal,
    CondBroadcast,
    CondWait,
    CondWaitDone,
    CondTimedWait,
    CondTimedWaitDone
};

#pragma pack(push, 1)
struct Event
{
    uint64_t timestamp;  // Nanoseconds since epoch
    uint32_t tid;  // Thread ID from gettid()
    EventType type;  // Event type
    void* ptr1;  // Primary pointer (mutex/rwlock/cond)
    void* ptr2;  // Secondary pointer (e.g. mutex for cond_wait)
    int32_t result;  // Return value
    uint64_t duration_ns;  // Duration for timed operations
    uint32_t stack_depth;  // Number of stack frames
};
#pragma pack(pop)

class VarIntWriter
{
  private:
    std::vector<uint8_t> buffer;

    void encodeVarInt(uint64_t value)
    {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value) byte |= 0x80;
            buffer.push_back(byte);
        } while (value);
    }

  public:
    void clear()
    {
        buffer.clear();
    }

    void write(uint64_t value)
    {
        encodeVarInt(value);
    }

    void write(EventType type)
    {
        buffer.push_back(static_cast<uint8_t>(type));
    }

    void writePtr(const void* ptr)
    {
        encodeVarInt(reinterpret_cast<uint64_t>(ptr));
    }

    void writeStack(void* const* stack, uint32_t depth)
    {
        encodeVarInt(depth);
        for (uint32_t i = 0; i < depth; i++) {
            encodeVarInt(reinterpret_cast<uint64_t>(stack[i]));
        }
    }

    const uint8_t* data() const
    {
        return buffer.data();
    }
    size_t size() const
    {
        return buffer.size();
    }
};

class EventLogger
{
  private:
    std::ofstream log_;
    std::atomic<bool> initialized_{false};
    static constexpr size_t MAX_STACK_DEPTH = 16;
    std::mutex write_mutex_;
    VarIntWriter writer_;

    EventLogger() = default;

  public:
    static EventLogger& instance()
    {
        static EventLogger logger;
        return logger;
    }

    void init(const char* filename = "/tmp/skeleton_key.bin")
    {
        if (!initialized_.exchange(true)) {
            log_.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
        }
    }

    void log(EventType type, void* ptr1, void* ptr2, int32_t result, uint64_t duration_ns = 0)
    {
        if (!initialized_) return;

        std::lock_guard<std::mutex> lock(write_mutex_);
        // Capture stack trace
        std::array<void*, MAX_STACK_DEPTH> stack;
        int depth = backtrace(stack.data(), MAX_STACK_DEPTH);

        // Get current time
        uint64_t timestamp =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());

        // Get thread ID
        uint32_t tid = static_cast<uint32_t>(syscall(SYS_gettid));

        // Write event in varint format
        writer_.clear();
        writer_.write(timestamp);
        writer_.write(tid);
        writer_.write(type);
        writer_.writePtr(ptr1);
        writer_.writePtr(ptr2);
        writer_.write(static_cast<uint64_t>(result));
        writer_.write(duration_ns);
        writer_.writeStack(stack.data(), depth);

        log_.write(reinterpret_cast<const char*>(writer_.data()), writer_.size());
        log_.flush();
    }

    ~EventLogger()
    {
        if (initialized_) {
            log_.close();
        }
    }
};

}  // namespace skeleton_key

// Function pointer declarations
extern "C" {
static int (*real_pthread_mutex_init)(pthread_mutex_t*, const pthread_mutexattr_t*) = nullptr;
static int (*real_pthread_mutex_destroy)(pthread_mutex_t*) = nullptr;
static int (*real_pthread_mutex_lock)(pthread_mutex_t*) = nullptr;
static int (*real_pthread_mutex_trylock)(pthread_mutex_t*) = nullptr;
static int (*real_pthread_mutex_timedlock)(pthread_mutex_t*, const struct timespec*) = nullptr;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t*) = nullptr;
static int (*real_pthread_cond_init)(pthread_cond_t*, const pthread_condattr_t*) = nullptr;
static int (*real_pthread_cond_destroy)(pthread_cond_t*) = nullptr;
static int (*real_pthread_cond_signal)(pthread_cond_t*) = nullptr;
static int (*real_pthread_cond_broadcast)(pthread_cond_t*) = nullptr;
static int (*real_pthread_cond_wait)(pthread_cond_t*, pthread_mutex_t*) = nullptr;
static int (*real_pthread_cond_timedwait)(pthread_cond_t*, pthread_mutex_t*, const struct timespec*) =
        nullptr;
static int (*real_pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) = nullptr;
static int (*real_pthread_rwlock_init)(pthread_rwlock_t*, const pthread_rwlockattr_t*) = nullptr;
static int (*real_pthread_rwlock_destroy)(pthread_rwlock_t*) = nullptr;
static int (*real_pthread_rwlock_rdlock)(pthread_rwlock_t*) = nullptr;
static int (*real_pthread_rwlock_tryrdlock)(pthread_rwlock_t*) = nullptr;
static int (*real_pthread_rwlock_timedrdlock)(pthread_rwlock_t*, const struct timespec*) = nullptr;
static int (*real_pthread_rwlock_wrlock)(pthread_rwlock_t*) = nullptr;
static int (*real_pthread_rwlock_trywrlock)(pthread_rwlock_t*) = nullptr;
static int (*real_pthread_rwlock_timedwrlock)(pthread_rwlock_t*, const struct timespec*) = nullptr;
static int (*real_pthread_rwlock_unlock)(pthread_rwlock_t*) = nullptr;
}

// Thread-local to prevent recursion
static thread_local bool in_hook = false;

// Library constructor
__attribute__((constructor)) static void
init_skeleton_key()
{
    printf("Initializing!\n");
    // Load versioned symbols for condition variables
    real_pthread_cond_init = reinterpret_cast<decltype(real_pthread_cond_init)>(
            dlvsym(RTLD_NEXT, "pthread_cond_init", "GLIBC_2.3.2"));
    real_pthread_cond_destroy = reinterpret_cast<decltype(real_pthread_cond_destroy)>(
            dlvsym(RTLD_NEXT, "pthread_cond_destroy", "GLIBC_2.3.2"));
    real_pthread_cond_signal = reinterpret_cast<decltype(real_pthread_cond_signal)>(
            dlvsym(RTLD_NEXT, "pthread_cond_signal", "GLIBC_2.3.2"));
    real_pthread_cond_broadcast = reinterpret_cast<decltype(real_pthread_cond_broadcast)>(
            dlvsym(RTLD_NEXT, "pthread_cond_broadcast", "GLIBC_2.3.2"));
    real_pthread_cond_wait = reinterpret_cast<decltype(real_pthread_cond_wait)>(
            dlvsym(RTLD_NEXT, "pthread_cond_wait", "GLIBC_2.3.2"));
    real_pthread_cond_timedwait = reinterpret_cast<decltype(real_pthread_cond_timedwait)>(
            dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.3.2"));

    // Load standard symbols
    real_pthread_mutex_init =
            reinterpret_cast<decltype(real_pthread_mutex_init)>(dlsym(RTLD_NEXT, "pthread_mutex_init"));
    real_pthread_mutex_destroy = reinterpret_cast<decltype(real_pthread_mutex_destroy)>(
            dlsym(RTLD_NEXT, "pthread_mutex_destroy"));
    real_pthread_mutex_lock =
            reinterpret_cast<decltype(real_pthread_mutex_lock)>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    real_pthread_mutex_trylock = reinterpret_cast<decltype(real_pthread_mutex_trylock)>(
            dlsym(RTLD_NEXT, "pthread_mutex_trylock"));
    real_pthread_mutex_timedlock = reinterpret_cast<decltype(real_pthread_mutex_timedlock)>(
            dlsym(RTLD_NEXT, "pthread_mutex_timedlock"));
    real_pthread_mutex_unlock = reinterpret_cast<decltype(real_pthread_mutex_unlock)>(
            dlsym(RTLD_NEXT, "pthread_mutex_unlock"));
    real_pthread_create =
            reinterpret_cast<decltype(real_pthread_create)>(dlsym(RTLD_NEXT, "pthread_create"));
    real_pthread_rwlock_init = reinterpret_cast<decltype(real_pthread_rwlock_init)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_init"));
    real_pthread_rwlock_destroy = reinterpret_cast<decltype(real_pthread_rwlock_destroy)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_destroy"));
    real_pthread_rwlock_rdlock = reinterpret_cast<decltype(real_pthread_rwlock_rdlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_rdlock"));
    real_pthread_rwlock_tryrdlock = reinterpret_cast<decltype(real_pthread_rwlock_tryrdlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_tryrdlock"));
    real_pthread_rwlock_timedrdlock = reinterpret_cast<decltype(real_pthread_rwlock_timedrdlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_timedrdlock"));
    real_pthread_rwlock_wrlock = reinterpret_cast<decltype(real_pthread_rwlock_wrlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_wrlock"));
    real_pthread_rwlock_trywrlock = reinterpret_cast<decltype(real_pthread_rwlock_trywrlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_trywrlock"));
    real_pthread_rwlock_timedwrlock = reinterpret_cast<decltype(real_pthread_rwlock_timedwrlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_timedwrlock"));
    real_pthread_rwlock_unlock = reinterpret_cast<decltype(real_pthread_rwlock_unlock)>(
            dlsym(RTLD_NEXT, "pthread_rwlock_unlock"));

    const char* filename = getenv("SKELETON_KEYOUTPUT");
    if (filename) {
        skeleton_key::EventLogger::instance().init(filename);
    } else {
        skeleton_key::EventLogger::instance().init();
    }
}

// Interposed pthread functions
extern "C" {

// Mutex functions
int
pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr)
{
    if (in_hook) return real_pthread_mutex_init(mutex, attr);
    in_hook = true;
    int result = real_pthread_mutex_init(mutex, attr);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexInit, mutex, nullptr, result);
    in_hook = false;
    return result;
}

int
pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    if (in_hook) return real_pthread_mutex_destroy(mutex);
    in_hook = true;
    int result = real_pthread_mutex_destroy(mutex);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexDestroy, mutex, nullptr, result);
    in_hook = false;
    return result;
}

int
pthread_mutex_lock(pthread_mutex_t* mutex)
{
    if (in_hook) return real_pthread_mutex_lock(mutex);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::MutexLock, mutex, nullptr, 0);

    int result = real_pthread_mutex_lock(mutex);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexLockDone, mutex, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_mutex_trylock(pthread_mutex_t* mutex)
{
    if (in_hook) return real_pthread_mutex_trylock(mutex);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::MutexTryLock, mutex, nullptr, 0);

    int result = real_pthread_mutex_trylock(mutex);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexTryLockDone, mutex, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_mutex_timedlock(pthread_mutex_t* mutex, const struct timespec* abstime)
{
    if (in_hook) return real_pthread_mutex_timedlock(mutex, abstime);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexTimedLock, mutex, nullptr, 0);

    int result = real_pthread_mutex_timedlock(mutex, abstime);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexTimedLockDone, mutex, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    if (in_hook) return real_pthread_mutex_unlock(mutex);
    in_hook = true;

    int result = real_pthread_mutex_unlock(mutex);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::MutexUnlock, mutex, nullptr, result);

    in_hook = false;
    return result;
}

// Condition variable functions
int
pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr)
{
    if (in_hook) return real_pthread_cond_init(cond, attr);
    in_hook = true;

    int result = real_pthread_cond_init(cond, attr);
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::CondInit, cond, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_cond_destroy(pthread_cond_t* cond)
{
    if (in_hook) return real_pthread_cond_destroy(cond);
    in_hook = true;

    int result = real_pthread_cond_destroy(cond);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::CondDestroy, cond, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_cond_signal(pthread_cond_t* cond)
{
    if (in_hook) return real_pthread_cond_signal(cond);
    in_hook = true;

    int result = real_pthread_cond_signal(cond);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::CondSignal, cond, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_cond_broadcast(pthread_cond_t* cond)
{
    if (in_hook) return real_pthread_cond_broadcast(cond);
    in_hook = true;

    int result = real_pthread_cond_broadcast(cond);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::CondBroadcast, cond, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    if (in_hook) return real_pthread_cond_wait(cond, mutex);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::CondWait, cond, mutex, 0);

    int result = real_pthread_cond_wait(cond, mutex);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::CondWaitDone, cond, mutex, result, duration);

    in_hook = false;
    return result;
}

int
pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime)
{
    if (in_hook) return real_pthread_cond_timedwait(cond, mutex, abstime);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::CondTimedWait, cond, mutex, 0);

    int result = real_pthread_cond_timedwait(cond, mutex, abstime);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::CondTimedWaitDone, cond, mutex, result, duration);

    in_hook = false;
    return result;
}

// RWLock functions
int
pthread_rwlock_init(pthread_rwlock_t* rwlock, const pthread_rwlockattr_t* attr)
{
    if (in_hook) return real_pthread_rwlock_init(rwlock, attr);
    in_hook = true;

    int result = real_pthread_rwlock_init(rwlock, attr);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockInit, rwlock, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_rwlock_destroy(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_destroy(rwlock);
    in_hook = true;

    int result = real_pthread_rwlock_destroy(rwlock);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockDestroy, rwlock, nullptr, result);

    in_hook = false;
    return result;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_rdlock(rwlock);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::RWLockRead, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_rdlock(rwlock);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockReadDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_tryrdlock(rwlock);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTryRead, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_tryrdlock(rwlock);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTryReadDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_timedrdlock(pthread_rwlock_t* rwlock, const struct timespec* abstime)
{
    if (in_hook) return real_pthread_rwlock_timedrdlock(rwlock, abstime);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTimedRead, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_timedrdlock(rwlock, abstime);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTimedReadDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_wrlock(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_wrlock(rwlock);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance().log(skeleton_key::EventType::RWLockWrite, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_wrlock(rwlock);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockWriteDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_trywrlock(rwlock);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTryWrite, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_trywrlock(rwlock);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTryWriteDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_timedwrlock(pthread_rwlock_t* rwlock, const struct timespec* abstime)
{
    if (in_hook) return real_pthread_rwlock_timedwrlock(rwlock, abstime);
    in_hook = true;

    auto start = std::chrono::steady_clock::now();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTimedWrite, rwlock, nullptr, 0);

    int result = real_pthread_rwlock_timedwrlock(rwlock, abstime);

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockTimedWriteDone, rwlock, nullptr, result, duration);

    in_hook = false;
    return result;
}

int
pthread_rwlock_unlock(pthread_rwlock_t* rwlock)
{
    if (in_hook) return real_pthread_rwlock_unlock(rwlock);
    in_hook = true;

    int result = real_pthread_rwlock_unlock(rwlock);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::RWLockUnlock, rwlock, nullptr, result);

    in_hook = false;
    return result;
}

// Thread creation
int
pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start_routine)(void*), void* arg)
{
    if (in_hook) return real_pthread_create(thread, attr, start_routine, arg);
    in_hook = true;

    int result = real_pthread_create(thread, attr, start_routine, arg);
    skeleton_key::EventLogger::instance()
            .log(skeleton_key::EventType::ThreadCreate, thread, nullptr, result);

    in_hook = false;
    return result;
}

}  // extern "C"
