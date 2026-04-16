#include <iostream>
#include <iomanip>
#include <string>
#include <queue>
#include <cstdint>

using namespace std;

const int CACHE_SETS = 4;
const int MEM_DELAY = 2;

enum class state
{
    IDLE,
    COMPARE_TAG,
    WRITE_BACK,
    ALLOCATE
};
enum class operation_type
{
    READ,
    WRITE
};

struct CPU_request
{
    operation_type op;
    unsigned int address;
    int data;
};

struct cache_block
{
    bool valid = false;
    bool dirty = false;
    unsigned int tag = 0;
    int data = 0;
};

string stateName(state s)
{
    switch (s)
    {
    case state::IDLE:
        return "IDLE";
    case state::COMPARE_TAG:
        return "COMPARE_TAG";
    case state::WRITE_BACK:
        return "WRITE_BACK";
    case state::ALLOCATE:
        return "ALLOCATE";
    }
    return "?";
}

unsigned int getIndex(unsigned int addr) { return addr & 0x03; }      // the last 2 bits determine the index
unsigned int getTag(unsigned int addr) { return (addr >> 2) & 0x3F; } // the upper 6 bits determine the tag

struct Memory
{
    int cells[256];
    Memory()
    {
        for (int i = 0; i < 256; ++i)
            cells[i] = i * 10;
    }

    bool read(unsigned int addr, int &out, int &cycleLeft)
    {
        if (cycleLeft > 0)
        {
            --cycleLeft;
            return false;
        }
        out = cells[addr]; // simulate reading from memory when the delay is over
        return true;
    }
    bool write(unsigned int addr, int val, int &cycleLeft)
    {
        if (cycleLeft > 0)
        {
            --cycleLeft;
            return false;
        }
        cells[addr] = val; // simulate writing to memory when the delay is over
        return true;
    }
};

struct CacheController
{
    state state = state::IDLE;
    cache_block cache[CACHE_SETS]; // cache with 4 sets, each block has valid, dirty, tag, and data fields...sets mean the number of blocks in the cache, since it's direct-mapped
    Memory mem;
    bool cacheReady = false;
    CPU_request curReq = {operation_type::READ, 0, 0}; // current request being processed
    int memCycles = 0;                                 // cycles left for memory operation to complete
    unsigned int wbTag = 0;
    int hits = 0, misses = 0, writebacks = 0;

    void tick(queue<CPU_request> &cpuQueue, int cycle)
    {
        // each tick is for 1 cycle of FSM
        cacheReady = false;
        cout << "\n[Cycle " << setw(2) << cycle << "] "
             << "state: " << left << setw(12) << stateName(state); // printing current state

        switch (state) // FSM transitions based on current state and inputs
        {

        case state::IDLE: // for IDLE, if there is a request in the CPU queue, we take it and move to COMPARE_TAG, otherwise we stay in IDLE
            if (!cpuQueue.empty())
            {
                curReq = cpuQueue.front();
                cpuQueue.pop();
                cout << "| CPU " << (curReq.op == operation_type::READ ? "READ " : "WRITE")
                     << " addr=0x" << hex << (int)curReq.address << dec
                     << "  ->  COMPARE_TAG";
                state = state::COMPARE_TAG;
            }
            else
                cout << "| (no request)";
            break;

        case state::COMPARE_TAG: // for COMPARE_TAG, we check if the requested address hits in the cache. If it's a hit, we serve it and go back to IDLE. If it's a miss, we check if the block to be replaced is dirty. If dirty, we go to WRITE_BACK, otherwise we go to ALLOCATE
        {
            unsigned int idx = getIndex(curReq.address);
            unsigned int tag = getTag(curReq.address);
            cache_block &blk = cache[idx];
            bool hit = blk.valid && (blk.tag == tag);

            cout << "| idx=" << (int)idx
                 << " tag=0x" << hex << (int)tag
                 << " cached_tag=0x" << (int)blk.tag << dec
                 << " valid=" << blk.valid
                 << " -> " << (hit ? "HIT" : "MISS");

            if (hit) // if it's a hit, we update the block if it's a write, and then go back to IDLE
            {
                ++hits;
                blk.valid = true;
                blk.tag = tag;
                if (curReq.op == operation_type::WRITE)
                {
                    blk.data = curReq.data;
                    blk.dirty = true;
                    cout << "  write=" << curReq.data << " dirty=1";
                }
                else
                {
                    cout << "  read data=" << blk.data;
                }
                cacheReady = true;
                cout << " CacheReady=1 -> IDLE";
                state = state::IDLE;
            }
            else // if it's a miss, we need to evict the block at idx. If it's dirty, we need to write it back first, then allocate the new block. If it's clean, we can directly allocate the new block
            {
                ++misses;
                wbTag = blk.tag;
                blk.tag = tag;
                if (blk.dirty)
                {
                    cout << "  dirty block -> WRITE_BACK";
                    memCycles = MEM_DELAY;
                    state = state::WRITE_BACK;
                }
                else
                {
                    cout << "  clean block -> ALLOCATE";
                    memCycles = MEM_DELAY;
                    state = state::ALLOCATE;
                }
            }
            break;
        }

        case state::WRITE_BACK: // for WRITE_BACK, we write the dirty block back to memory. Once the write is done, we move to ALLOCATE to fetch the new block
        {
            unsigned int idx = getIndex(curReq.address);
            cache_block &blk = cache[idx];
            unsigned int wbAddr = (wbTag << 2) | idx;
            bool done = mem.write(wbAddr, blk.data, memCycles);

            cout << "| wb_addr=0x" << hex << (int)wbAddr << dec
                 << " data=" << blk.data
                 << " mem_cycles_left=" << memCycles;
            if (done)
            {
                ++writebacks;
                blk.dirty = false;
                cout << " MemReady -> ALLOCATE";
                memCycles = MEM_DELAY;
                state = state::ALLOCATE;
            }
            else
                cout << " (waiting)";
            break;
        }

        case state::ALLOCATE:
        {
            unsigned int idx = getIndex(curReq.address);
            cache_block &blk = cache[idx];
            int newData = 0;
            bool done = mem.read(curReq.address, newData, memCycles);

            cout << "| fetch addr=0x" << hex << (int)curReq.address << dec
                 << " mem_cycles_left=" << memCycles;
            if (done)
            {
                blk.data = newData;
                blk.valid = true;
                blk.dirty = false;
                cout << " data=" << newData << " MemReady -> COMPARE_TAG";
                state = state::COMPARE_TAG;
            }
            else
                cout << " (waiting)";
            break;
        }
        }
    }

    void printCache()
    {
        cout << "\n\n+------+-------+-------+-------+--------+\n"
             << "| Set  | Valid | Dirty |  Tag  |  Data  |\n"
             << "+------+-------+-------+-------+--------+\n";
        for (int i = 0; i < CACHE_SETS; ++i)
        {
            cache_block &b = cache[i];
            cout << "|  " << i << "   |   " << b.valid
                 << "   |   " << b.dirty
                 << "   |  0x" << hex << setw(2) << setfill('0') << (int)b.tag
                 << dec << setfill(' ')
                 << " | " << setw(6) << b.data << " |\n";
        }
        cout << "+------+-------+-------+-------+--------+\n";
    }

    void printStats()
    {
        int total = hits + misses;
        cout << "\n=== Stats ===\n"
             << "  Hits        : " << hits << "\n"
             << "  Misses      : " << misses << "\n"
             << "  Write-backs : " << writebacks << "\n"
             << "  Hit rate    : "
             << fixed << setprecision(1)
             << (total > 0 ? 100.0 * hits / total : 0.0) << "%\n";
    }
};

void enqueue(queue<CPU_request> &q, operation_type op, unsigned int addr, int data = 0)
{
    CPU_request r;
    r.op = op;
    r.address = addr;
    r.data = data;
    q.push(r);
}

int main()
{
    CacheController ctrl;
    queue<CPU_request> cpuQueue;

    cout << "=== FSM Cache Controller Simulation ===\n"
         << "Config : " << CACHE_SETS << " sets, direct-mapped, 1-word blocks\n"
         << "Address: [tag bits 7-2] [index bits 1-0]\n"
         << "Memory : " << MEM_DELAY << " cycle latency\n\n"
         << "Requests:\n"
         << "  1. READ  0x04  idx=0 tag=1  => cold miss\n"
         << "  2. READ  0x04  idx=0 tag=1  => hit\n"
         << "  3. WRITE 0x04  idx=0 tag=1  => write hit, dirty\n"
         << "  4. READ  0x08  idx=0 tag=2  => conflict miss + writeback\n"
         << "  5. READ  0x05  idx=1 tag=1  => cold miss, different set\n"
         << "  6. WRITE 0x09  idx=1 tag=2  => write miss, clean block\n";

    enqueue(cpuQueue, operation_type::READ, 0x04);
    enqueue(cpuQueue, operation_type::READ, 0x04);
    enqueue(cpuQueue, operation_type::WRITE, 0x04, 999);
    enqueue(cpuQueue, operation_type::READ, 0x08);
    enqueue(cpuQueue, operation_type::READ, 0x05);
    enqueue(cpuQueue, operation_type::WRITE, 0x09, 42);

    // alt example:
    // Covers cold misses, repeated hits, dirty write-backs, and set conflicts across multiple indices.
    // enqueue(cpuQueue, operation_type::READ,  0x00);       // cold miss set 0, tag 0
    // enqueue(cpuQueue, operation_type::WRITE, 0x00, 111);  // write hit, dirty set 0
    // enqueue(cpuQueue, operation_type::READ,  0x10);       // conflict miss set 0, dirty eviction of 0x00
    // enqueue(cpuQueue, operation_type::READ,  0x04);       // conflict miss set 0 again
    // enqueue(cpuQueue, operation_type::WRITE, 0x04, 222);  // write hit, dirty set 0
    // enqueue(cpuQueue, operation_type::READ,  0x14);       // conflict miss set 0, dirty eviction of 0x04
    // enqueue(cpuQueue, operation_type::READ,  0x01);       // cold miss set 1, tag 0
    // enqueue(cpuQueue, operation_type::WRITE, 0x01, 333);  // write hit, dirty set 1
    // enqueue(cpuQueue, operation_type::READ,  0x11);       // conflict miss set 1, dirty eviction of 0x01
    // enqueue(cpuQueue, operation_type::READ,  0x05);       // conflict miss set 1
    // enqueue(cpuQueue, operation_type::WRITE, 0x05, 444);  // write hit, dirty set 1
    // enqueue(cpuQueue, operation_type::READ,  0x21);       // conflict miss set 1, dirty eviction of 0x05
    // enqueue(cpuQueue, operation_type::READ,  0x02);       // cold miss set 2
    // enqueue(cpuQueue, operation_type::READ,  0x02);       // immediate hit set 2
    // enqueue(cpuQueue, operation_type::READ,  0x03);       // cold miss set 3
    // enqueue(cpuQueue, operation_type::WRITE, 0x03, 555);  // write hit, dirty set 3
    // enqueue(cpuQueue, operation_type::READ,  0x13);       // conflict miss set 3, dirty eviction of 0x03

    int cycle = 1;
    while (!cpuQueue.empty() || ctrl.state != state::IDLE)
    {
        ctrl.tick(cpuQueue, cycle++);
        if (cycle > 100)
            break;
    }

    cout << "\n\n=== Final Cache state ===";
    ctrl.printCache();
    ctrl.printStats();
    return 0;
}