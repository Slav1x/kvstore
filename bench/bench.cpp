// Measures write, read, and compaction speed.
//
//   ./kvstore_bench [number of keys]    (default: 100000)
//
// Build in Release mode for real numbers.
#include "kvstore.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static void report(const char* label, std::size_t ops, double secs) {
    std::printf("%-22s %8zu ops  %7.3f s  %12.0f ops/sec\n", label, ops, secs, ops / secs);
}

int main(int argc, char** argv) {
    std::size_t n = argc > 1 ? std::stoul(argv[1]) : 100000;
    std::string path = (fs::temp_directory_path() / "kvstore_bench.log").string();
    fs::remove(path);

    // Keys like "key000042", values are 100 bytes.
    std::vector<std::string> keys;
    keys.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::string k = std::to_string(i);
        keys.push_back("key" + std::string(6 - std::min<std::size_t>(6, k.size()), '0') + k);
    }
    std::string value(100, 'x');

    std::mt19937 rng(42);  // fixed seed so runs are comparable
    std::uniform_int_distribution<std::size_t> pick(0, n - 1);

    std::cout << "kvstore benchmark: " << n << " keys, 100-byte values\n\n";

    KVStore db(path);

    auto t = Clock::now();
    for (const auto& k : keys) db.put(k, value);
    report("sequential writes", n, secondsSince(t));

    t = Clock::now();
    std::size_t found = 0;
    for (std::size_t i = 0; i < n; ++i) found += db.get(keys[pick(rng)]).has_value();
    report("random reads", n, secondsSince(t));
    if (found != n) std::cerr << "warning: only found " << found << " keys\n";

    t = Clock::now();
    for (std::size_t i = 0; i < n; ++i) db.put(keys[pick(rng)], value);
    report("random overwrites", n, secondsSince(t));

    auto before = db.logBytes();
    t = Clock::now();
    db.compact();
    double compactSecs = secondsSince(t);
    std::printf("\ncompaction: %.1f MB -> %.1f MB in %.3f s\n", before / 1e6,
                db.logBytes() / 1e6, compactSecs);

    t = Clock::now();
    KVStore reopened(path);
    std::printf("startup replay: %zu keys in %.3f s\n", reopened.size(), secondsSince(t));

    fs::remove(path);
}
