#include <chrono>
#include <cstring>
#include <execinfo.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

// Keep enum in sync with writer
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

class VarIntReader
{
    std::vector<uint8_t> buffer_;
    size_t pos_ = 0;

  public:
    explicit VarIntReader(std::vector<uint8_t>&& buffer)
    : buffer_(std::move(buffer))
    {
    }

    uint64_t readVarInt()
    {
        uint64_t result = 0;
        int shift = 0;

        while (pos_ < buffer_.size()) {
            uint8_t byte = buffer_[pos_++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }

        return result;
    }

    EventType readEventType()
    {
        return static_cast<EventType>(buffer_[pos_++]);
    }

    void* readPtr()
    {
        return reinterpret_cast<void*>(readVarInt());
    }

    std::vector<void*> readStack()
    {
        uint32_t depth = readVarInt();
        std::vector<void*> stack;
        stack.reserve(depth);

        for (uint32_t i = 0; i < depth; i++) {
            stack.push_back(readPtr());
        }

        return stack;
    }

    bool eof() const
    {
        return pos_ >= buffer_.size();
    }
};

std::string
eventTypeToString(EventType type)
{
    switch (type) {
        case EventType::ThreadCreate:
            return "ThreadCreate";
        case EventType::MutexInit:
            return "MutexInit";
        case EventType::MutexDestroy:
            return "MutexDestroy";
        case EventType::MutexLock:
            return "MutexLock";
        case EventType::MutexLockDone:
            return "MutexLockDone";
        case EventType::MutexTryLock:
            return "MutexTryLock";
        case EventType::MutexTryLockDone:
            return "MutexTryLockDone";
        case EventType::MutexTimedLock:
            return "MutexTimedLock";
        case EventType::MutexTimedLockDone:
            return "MutexTimedLockDone";
        case EventType::MutexUnlock:
            return "MutexUnlock";
        case EventType::RWLockInit:
            return "RWLockInit";
        case EventType::RWLockDestroy:
            return "RWLockDestroy";
        case EventType::RWLockRead:
            return "RWLockRead";
        case EventType::RWLockReadDone:
            return "RWLockReadDone";
        case EventType::RWLockTryRead:
            return "RWLockTryRead";
        case EventType::RWLockTryReadDone:
            return "RWLockTryReadDone";
        case EventType::RWLockTimedRead:
            return "RWLockTimedRead";
        case EventType::RWLockTimedReadDone:
            return "RWLockTimedReadDone";
        case EventType::RWLockWrite:
            return "RWLockWrite";
        case EventType::RWLockWriteDone:
            return "RWLockWriteDone";
        case EventType::RWLockTryWrite:
            return "RWLockTryWrite";
        case EventType::RWLockTryWriteDone:
            return "RWLockTryWriteDone";
        case EventType::RWLockTimedWrite:
            return "RWLockTimedWrite";
        case EventType::RWLockTimedWriteDone:
            return "RWLockTimedWriteDone";
        case EventType::RWLockUnlock:
            return "RWLockUnlock";
        case EventType::CondInit:
            return "CondInit";
        case EventType::CondDestroy:
            return "CondDestroy";
        case EventType::CondSignal:
            return "CondSignal";
        case EventType::CondBroadcast:
            return "CondBroadcast";
        case EventType::CondWait:
            return "CondWait";
        case EventType::CondWaitDone:
            return "CondWaitDone";
        case EventType::CondTimedWait:
            return "CondTimedWait";
        case EventType::CondTimedWaitDone:
            return "CondTimedWaitDone";
        default:
            return "Unknown";
    }
}

int
main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <trace file>\n";
        return 1;
    }

    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "Failed to open " << argv[1] << "\n";
        return 1;
    }

    // Read entire file
    input.seekg(0, std::ios::end);
    size_t size = input.tellg();
    input.seekg(0);

    std::vector<uint8_t> buffer(size);
    input.read(reinterpret_cast<char*>(buffer.data()), size);

    VarIntReader reader(std::move(buffer));
    uint64_t first_timestamp = 0;

    // Process events
    while (!reader.eof()) {
        uint64_t timestamp = reader.readVarInt();
        if (first_timestamp == 0) first_timestamp = timestamp;
        uint32_t tid = reader.readVarInt();
        EventType type = reader.readEventType();
        void* ptr1 = reader.readPtr();
        void* ptr2 = reader.readPtr();
        int32_t result = reader.readVarInt();
        uint64_t duration = reader.readVarInt();
        auto stack = reader.readStack();

        // Print event details
        std::cout << std::fixed << std::setprecision(6) << (timestamp - first_timestamp) / 1e9 << " "
                  << "tid=" << tid << " " << std::setw(20) << std::left << eventTypeToString(type) << " "
                  << "ptr=" << ptr1;

        if (ptr2) {
            std::cout << " aux_ptr=" << ptr2;
        }

        if (duration > 0) {
            std::cout << " duration=" << duration / 1e9 << "s";
        }

        if (result != 0) {
            std::cout << " result=" << result;
        }

        std::cout << "\nStack trace:\n";
        for (void* addr : stack) {
            std::cout << "  " << (void*)addr << "\n";
        }
        std::cout << "\n";
    }

    return 0;
}
