#pragma once

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

// A key-value store backed by an append-only log file.
//
// Every put/del is appended to the log before it changes the in-memory map.
// On startup the log is replayed from the beginning to rebuild the map.
// compact() rewrites the log with only the live keys.
class KVStore {
public:
    explicit KVStore(std::string path);

    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);

    // Rewrites the log so it only holds the current value of each live key.
    void compact();

    std::size_t size() const { return data_.size(); }
    std::uintmax_t logBytes() const;

private:
    void replay();
    void openLog();
    void append(char op, const std::string& key, const std::string& value);

    std::string path_;
    std::unordered_map<std::string, std::string> data_;
    std::ofstream log_;
};
