#ifndef HASHTABLE1_H
#define HASHTABLE1_H
#include <stdint.h>
#include <mintomic/mintomic.h>
class HashTable1
{
public:
    struct Entry
    {
        mint_atomic64_t key;
        mint_atomic64_t value;
    };

private:
    Entry* m_entries;
    uint64_t m_arraySize;

public:
    HashTable1(uint64_t arraySize);
    ~HashTable1();

    // Basic operations
    void SetItem(uint64_t key, uint64_t value);
    uint64_t GetItem(uint64_t key);
    uint64_t GetItemCount();
    void DeleteItem(uint64_t key);
    void Clear();
};


#endif // HASHTABLE1_H
