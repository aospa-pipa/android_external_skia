/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkLRUCache_DEFINED
#define SkLRUCache_DEFINED

#include "SkChecksum.h"
#include "SkTHash.h"
#include "SkTInternalLList.h"

/**
 * A generic LRU cache.
 */
template <typename K, typename V, typename HashK = SkGoodHash>
class SkLRUCache : public SkNoncopyable {
public:
    explicit SkLRUCache(int maxCount)
    : fMaxCount(maxCount) {}

    ~SkLRUCache() {
        Entry* node = fLRU.head();
        while (node) {
            fLRU.remove(node);
            delete node;
            node = fLRU.head();
        }
    }

    V* find(const K& key) {
        Entry** value = fMap.find(key);
        if (!value) {
            return nullptr;
        }
        Entry* entry = *value;
        if (entry != fLRU.head()) {
            fLRU.remove(entry);
            fLRU.addToHead(entry);
        } // else it's already at head position, don't need to do anything
        return &entry->fValue;
    }

    V* insert(const K& key, V value) {
        Entry* entry = new Entry(key, std::move(value));
        fMap.set(entry);
        fLRU.addToHead(entry);
        while (fMap.count() > fMaxCount) {
            this->remove(fLRU.tail()->fKey);
        }
        return &entry->fValue;
    }

    int count() {
        return fMap.count();
    }

private:
    struct Entry {
        Entry(const K& key, V&& value)
        : fKey(key)
        , fValue(std::move(value)) {}

        K fKey;
        V fValue;

        SK_DECLARE_INTERNAL_LLIST_INTERFACE(Entry);
    };

    struct Traits {
        static const K& GetKey(Entry* e) {
            return e->fKey;
        }

        static uint32_t Hash(const K& k) {
            return HashK()(k);
        }
    };

    void remove(const K& key) {
        Entry** value = fMap.find(key);
        SkASSERT(value);
        Entry* entry = *value;
        SkASSERT(key == entry->fKey);
        fMap.remove(key);
        fLRU.remove(entry);
        delete entry;
    }

    int                             fMaxCount;
    SkTHashTable<Entry*, K, Traits> fMap;
    SkTInternalLList<Entry>         fLRU;
};

#endif