#pragma once

#include <cstdint>
#include <string>
#include <iostream>
#include <list>
#include <unordered_map>
#include <memory>

namespace punkt {
bool caseInsensitiveEquals(std::string_view lhs, std::string_view rhs);

size_t stringViewToSizeTHex(const std::string_view &sv, const std::string_view &attr_name);

size_t stringViewToSizeT(const std::string_view &sv, const std::string_view &attr_name);

float stringViewToFloat(const std::string_view &sv, const std::string_view &attr_name);

void parseColor(const std::string_view &color, uint8_t &r, uint8_t &g, uint8_t &b);

template<typename Key, typename Value, typename KeyHasher, size_t DefaultCapacity = 256>
struct LRUCache {
    using KeyValuePair = std::pair<Key, std::unique_ptr<Value> >;
    using ListIterator = typename std::list<KeyValuePair>::iterator;

    explicit LRUCache()
        : m_capacity(DefaultCapacity) {
    }

    explicit LRUCache(const size_t capacity)
        : m_capacity(capacity) {
    }

    // Get an item and mark it as recently used
    Value *get(const Key &key) {
        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            return nullptr; // Not found
        }

        // Move accessed item to the front of the list
        m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second);
        return it->second->second.get();
    }

    bool contains(const Key &key) {
        return get(key) != nullptr;
    }

    Value &at(const Key &key) {
        auto out = get(key);
        if (out == nullptr) {
            throw std::out_of_range("key does not exist");
        }
        return *out;
    }

    // Insert a new item (or update if it exists)
    void put(const Key &key, std::unique_ptr<Value> value) {
        auto it = m_cache_map.find(key);
        if (it != m_cache_map.end()) {
            // Update existing value and move to front
            it->second->second = std::move(value);
            m_cache_list.splice(m_cache_list.begin(), m_cache_list, it->second);
            return;
        }

        // Evict LRU item if at capacity
        if (m_cache_list.size() >= m_capacity) {
            evict();
        }

        // Insert new entry at front
        m_cache_list.emplace_front(key, std::move(value));
        m_cache_map[key] = m_cache_list.begin();
    }

    // Remove LRU item
    void evict() {
        if (m_cache_list.empty()) return;

        auto &last = m_cache_list.back(); // The least recently used item
        m_cache_map.erase(last.first);
        m_cache_list.pop_back(); // Destructor of unique_ptr is called here
    }

    // Print cache content (for debugging)
    void print() const {
        std::cout << "Cache: ";
        for (const auto &[key, value]: m_cache_list) {
            std::cout << key << " ";
        }
        std::cout << "\n";
    }

private:
    size_t m_capacity;
    std::list<KeyValuePair> m_cache_list; // Stores key-value pairs (ordered by usage)
    std::unordered_map<Key, ListIterator, KeyHasher> m_cache_map; // Maps keys to list iterators
};
}

#define getAttrOrDefault(attrs, attr_name, default_value) ((attrs).contains(attr_name) ? (attrs).at(attr_name) : (default_value))
#define getAttrTransformedOrDefault(attrs, attr_name, default_value, transformation) ((attrs).contains(attr_name) ? transformation((attrs).at(attr_name)) : (default_value))
#define getAttrTransformedCheckedOrDefault(attrs, attr_name, default_value, transformation) ((attrs).contains(attr_name) ? transformation((attrs).at(attr_name), (attr_name)) : (default_value))
