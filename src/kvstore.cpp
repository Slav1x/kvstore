#include "kvstore.h"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

// Log record format (one record per put/del):
//
//   <op> <key length> <value length>\n<key bytes><value bytes>\n
//
//   op is 'P' for put or 'D' for delete (deletes have value length 0).
//
// Lengths are stored up front so keys and values can contain spaces,
// newlines, or anything else. The trailing '\n' marks a record as complete,
// which is how replay() spots a record that was cut off by a crash.

// Writes one record in the format above. Used by both append() and compact().
static void writeRecord(std::ostream& out, char op, const std::string& key,
                        const std::string& value) {
    out << op << ' ' << key.size() << ' ' << value.size() << '\n'
        << key << value << '\n';
}

KVStore::KVStore(std::string path) : path_(std::move(path)) {
    replay();
    openLog();
}

void KVStore::put(const std::string& key, const std::string& value) {
    append('P', key, value);  // write to disk first, then update memory
    data_[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    if (data_.find(key) == data_.end()) return false;
    append('D', key, "");
    data_.erase(key);
    return true;
}

void KVStore::compact() {
    // Write every live key into a new file, then swap it in for the old log.
    // If we crash halfway, the old log is still intact, so nothing is lost.
    std::string tmpPath = path_ + ".compact";
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("kvstore: cannot create " + tmpPath);
        for (const auto& [key, value] : data_) {
            writeRecord(out, 'P', key, value);
        }
        out.flush();
        if (!out) throw std::runtime_error("kvstore: compaction write failed");
    }

    log_.close();
    fs::rename(tmpPath, path_);  // replaces the old log in one step
    openLog();
}

std::uintmax_t KVStore::logBytes() const {
    std::error_code ec;
    auto n = fs::file_size(path_, ec);
    return ec ? 0 : n;
}

void KVStore::openLog() {
    log_.open(path_, std::ios::binary | std::ios::app);
    if (!log_) throw std::runtime_error("kvstore: cannot open log file " + path_);
}

void KVStore::append(char op, const std::string& key, const std::string& value) {
    writeRecord(log_, op, key, value);
    log_.flush();  // hand the bytes to the OS right away (see README: durability)
    if (!log_) throw std::runtime_error("kvstore: write to log failed");
}

void KVStore::replay() {
    std::ifstream in(path_, std::ios::binary);
    if (!in) return;  // no log yet, start empty

    std::streamoff lastGood = 0;  // end of the last complete record

    while (true) {
        char op;
        std::size_t klen, vlen;
        if (!(in >> op >> klen >> vlen)) break;
        if (in.get() != '\n') break;

        std::string key(klen, '\0'), value(vlen, '\0');
        if (!in.read(key.data(), klen)) break;
        if (!in.read(value.data(), vlen)) break;
        if (in.get() != '\n') break;

        if (op == 'P') data_[key] = value;
        else if (op == 'D') data_.erase(key);
        else break;  // unknown op means the file is damaged from here on

        lastGood = in.tellg();
    }
    in.close();

    // Anything after the last complete record is a partial write from a crash.
    // Cut it off so new records don't get glued onto garbage.
    if (static_cast<std::uintmax_t>(lastGood) < fs::file_size(path_)) {
        fs::resize_file(path_, static_cast<std::uintmax_t>(lastGood));
    }
}
