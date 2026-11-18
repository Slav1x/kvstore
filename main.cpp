#include <iostream>
#include <string>
#include <unordered_map>
#include <optional>

class KVStore {
public:
    void put(const std::string& key, const std::string& value) {
        data_[key] = value;
    }

    std::optional<std::string> get(const std::string& key) const {
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    bool del(const std::string& key) {
        return data_.erase(key) > 0;
    }

private:
    std::unordered_map<std::string, std::string> data_;
};

int main() {
    KVStore db;
    db.put("name", "slavik");
    std::cout << db.get("name").value_or("(not found)") << "\n";
    db.del("name");
    std::cout << db.get("name").value_or("(not found)") << "\n";
}
