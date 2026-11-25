# kvstore
A key-value storage engine in C++17. Think a tiny Redis that saves to disk: put, get, and delete values by key, and your data is still there after the program exits or crashes.

## Build and run
```bash
cmake -S . -B build
cmake --build build
./build/kvstore            # interactive CLI, data saved to kvstore.log
./build/kvstore_tests      # unit tests (or: cd build && ctest)
./build/kvstore_bench      # benchmark
```

## CLI
```
$ ./build/kvstore
kvstore v0.2 | kvstore.log | 0 keys loaded. type 'help' for commands.
> put name slavik
OK
> put motto build first
OK
> get motto
build first
> stats
2 keys, log is 56 bytes
> compact
log: 56 -> 39 bytes
```

## How it works
**Append-only log.** Every `put` and `del` is appended to the end of a log file before it changes the in-memory hash map. Nothing in the file is ever edited in place, so a write can never corrupt older data.

**Record format.** Each record stores its lengths up front, so keys and values can hold spaces, newlines, or anything else:
```
<op> <key length> <value length>\n<key><value>\n      op = P (put) or D (delete)
```

**Startup replay.** When the store opens, it reads the log from the top and replays every record into the hash map. Reads are then a single hash map lookup.

**Crash recovery.** If the program dies mid-write, the last record is incomplete. Replay stops at the last complete record and cuts the broken tail off the file, so the next write doesn't get glued onto garbage.

**Compaction.** Overwrites and deletes leave old records behind, so the log keeps growing. `compact()` writes just the live keys to a new file and renames it over the old log. If it crashes halfway, the old log is untouched.

## Benchmark
100,000 keys with 100-byte values, Release build:

| Operation | ops/sec |
|---|---|
| Sequential writes | ~870,000 |
| Random reads | ~3,000,000 |
| Random overwrites | ~920,000 |

- Compaction after 100k overwrites: 23.6 MB -> 11.8 MB in 0.04 s
- Startup replay of 100k keys: 0.07 s

Run on a Linux cloud VM. Numbers will vary by machine.

## Tradeoffs and limits
- **Durability:** writes are flushed to the OS after every operation, but not `fsync`ed. That survives the program crashing, but not the whole machine losing power. Adding `fsync` per write would be much safer and much slower.
- **Memory:** every key and value lives in RAM, so the dataset has to fit in memory (same idea as Bitcask).
- **Compaction is manual:** you call `compact` yourself; nothing triggers it automatically yet.
- **No checksums:** recovery catches cut-off records, but not a record whose bytes got flipped on disk.

## Roadmap
- [x] Project setup with CMake
- [x] In-memory put/get/delete
- [x] CLI interface
- [x] Unit tests (14 tests: core ops, persistence, crash recovery, compaction)
- [x] Disk persistence (append-only log)
- [x] Crash recovery on startup
- [x] Compaction
- [x] Benchmark (ops/sec for reads and writes)
- [ ] Checksums per record
- [ ] Automatic compaction when the log gets too big
- [ ] Networking (TCP server)
