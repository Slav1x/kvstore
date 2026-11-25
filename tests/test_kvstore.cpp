// Small self-contained test runner so the project has no outside dependencies.
#include "kvstore.h"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "    FAILED: " #cond " (line " << __LINE__ << ")\n"; \
            ++failures;                                                     \
        }                                                                   \
    } while (0)

// Each test gets its own fresh log file in the temp directory.
static std::string freshPath(const std::string& name) {
    fs::path p = fs::temp_directory_path() / ("kvstore_test_" + name + ".log");
    fs::remove(p);
    return p.string();
}

// ---------- in-memory behavior ----------

static void test_put_then_get() {
    KVStore db(freshPath("put_get"));
    db.put("name", "slavik");
    CHECK(db.get("name") == "slavik");
}

static void test_get_missing_key() {
    KVStore db(freshPath("missing"));
    CHECK(!db.get("nope").has_value());
}

static void test_put_overwrites() {
    KVStore db(freshPath("overwrite"));
    db.put("k", "v1");
    db.put("k", "v2");
    CHECK(db.get("k") == "v2");
    CHECK(db.size() == 1);
}

static void test_delete() {
    KVStore db(freshPath("delete"));
    db.put("k", "v");
    CHECK(db.del("k"));
    CHECK(!db.get("k").has_value());
    CHECK(!db.del("k"));  // deleting twice reports nothing was deleted
}

// ---------- disk persistence ----------

static void test_data_survives_reopen() {
    std::string path = freshPath("reopen");
    {
        KVStore db(path);
        db.put("a", "1");
        db.put("b", "2");
    }
    KVStore db(path);
    CHECK(db.get("a") == "1");
    CHECK(db.get("b") == "2");
}

static void test_overwrite_survives_reopen() {
    std::string path = freshPath("reopen_overwrite");
    {
        KVStore db(path);
        db.put("k", "old");
        db.put("k", "new");
    }
    KVStore db(path);
    CHECK(db.get("k") == "new");
}

static void test_delete_survives_reopen() {
    std::string path = freshPath("reopen_delete");
    {
        KVStore db(path);
        db.put("k", "v");
        db.del("k");
    }
    KVStore db(path);
    CHECK(!db.get("k").has_value());
}

static void test_keys_with_spaces_and_newlines() {
    std::string path = freshPath("weird_keys");
    {
        KVStore db(path);
        db.put("my key", "line1\nline2");
        db.put("", "empty key");
    }
    KVStore db(path);
    CHECK(db.get("my key") == "line1\nline2");
    CHECK(db.get("") == "empty key");
}

static void test_writes_hit_disk_immediately() {
    // Simulates a crash: the store is never closed, but a second instance
    // reading the same file should already see the write.
    std::string path = freshPath("flush");
    KVStore writer(path);
    writer.put("k", "v");
    KVStore reader(path);
    CHECK(reader.get("k") == "v");
}

// ---------- crash recovery ----------

// Chops `n` bytes off the end of the file, like a crash in the middle of a write.
static void chopEnd(const std::string& path, std::uintmax_t n) {
    fs::resize_file(path, fs::file_size(path) - n);
}

static void test_recovers_from_cut_off_record() {
    std::string path = freshPath("torn");
    {
        KVStore db(path);
        db.put("safe", "yes");
        db.put("torn", "this write gets cut off");
    }
    chopEnd(path, 5);
    KVStore db(path);
    CHECK(db.get("safe") == "yes");      // earlier records are fine
    CHECK(!db.get("torn").has_value());  // the partial record is ignored
}

static void test_can_write_after_recovery() {
    std::string path = freshPath("torn_then_write");
    {
        KVStore db(path);
        db.put("a", "1");
        db.put("b", "2");
    }
    chopEnd(path, 3);
    {
        KVStore db(path);
        db.put("c", "3");  // must not get glued onto the broken record
    }
    KVStore db(path);
    CHECK(db.get("a") == "1");
    CHECK(db.get("c") == "3");
    CHECK(db.size() == 2);
}

// ---------- compaction ----------

static void test_compaction_shrinks_log() {
    std::string path = freshPath("compact_shrink");
    KVStore db(path);
    for (int i = 0; i < 100; ++i) db.put("k", "value " + std::to_string(i));
    auto before = db.logBytes();
    db.compact();
    CHECK(db.logBytes() < before);
}

static void test_compaction_keeps_data() {
    std::string path = freshPath("compact_keep");
    {
        KVStore db(path);
        db.put("a", "old");
        db.put("a", "new");
        db.put("b", "2");
        db.put("gone", "x");
        db.del("gone");
        db.compact();
        CHECK(db.get("a") == "new");
    }
    KVStore db(path);  // reopen from the compacted file
    CHECK(db.get("a") == "new");
    CHECK(db.get("b") == "2");
    CHECK(!db.get("gone").has_value());
    CHECK(db.size() == 2);
}

static void test_writes_after_compaction_persist() {
    std::string path = freshPath("compact_then_write");
    {
        KVStore db(path);
        db.put("a", "1");
        db.compact();
        db.put("b", "2");
    }
    KVStore db(path);
    CHECK(db.get("a") == "1");
    CHECK(db.get("b") == "2");
}

int main() {
    std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"put then get", test_put_then_get},
        {"get missing key", test_get_missing_key},
        {"put overwrites", test_put_overwrites},
        {"delete", test_delete},
        {"data survives reopen", test_data_survives_reopen},
        {"overwrite survives reopen", test_overwrite_survives_reopen},
        {"delete survives reopen", test_delete_survives_reopen},
        {"keys with spaces and newlines", test_keys_with_spaces_and_newlines},
        {"writes hit disk immediately", test_writes_hit_disk_immediately},
        {"recovers from cut-off record", test_recovers_from_cut_off_record},
        {"can write after recovery", test_can_write_after_recovery},
        {"compaction shrinks log", test_compaction_shrinks_log},
        {"compaction keeps data", test_compaction_keeps_data},
        {"writes after compaction persist", test_writes_after_compaction_persist},
    };

    int passed = 0;
    for (auto& [name, fn] : tests) {
        int before = failures;
        fn();
        bool ok = failures == before;
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
        if (ok) ++passed;
    }
    std::cout << "\n" << passed << "/" << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
