#include "hashtable1.h"
#include <assert.h>
#include <memory.h>
#include "mintomic/mintomic.h"
#include "murmur_hash.h"

#define USE_FAST_SETITEM 1


//----------------------------------------------
// from code.google.com/p/smhasher/wiki/MurmurHash3
//inline static uint32_t integerHash(uint32_t h)
//{
//  return common::MurmurHash2()(&h, sizeof(h));
//}

inline static uint64_t integerHash (uint64_t h) {
  return common::MurmurHash64A()(&h, sizeof(h));
}



//----------------------------------------------
HashTable1::HashTable1(uint64_t arraySize)
{
    // Initialize cells
    assert((arraySize & (arraySize - 1)) == 0);   // Must be a power of 2
    m_arraySize = arraySize;
    m_entries = new Entry[arraySize];
    Clear();
}


//----------------------------------------------
HashTable1::~HashTable1()
{
    // Delete cells
    delete[] m_entries;
}


//----------------------------------------------
#if USE_FAST_SETITEM
void HashTable1::SetItem(uint64_t key, uint64_t value)
{
    assert(key != 0);
    assert(value != 0);

    for (uint64_t idx = integerHash(key);; idx++)
    {
        idx &= m_arraySize - 1;

        // Load the key that was there.
        uint64_t probedKey = mint_load_64_relaxed(&m_entries[idx].key);
        if (probedKey != key)
        {
            // The entry was either free, or contains another key.
            if (probedKey != 0)
                continue;           // Usually, it contains another key. Keep probing.

            // The entry was free. Now let's try to take it using a CAS.
            uint64_t prevKey = mint_compare_exchange_strong_64_relaxed(&m_entries[idx].key, 0, key);
            if ((prevKey != 0) && (prevKey != key))
                continue;       // Another thread just stole it from underneath us.

            // Either we just added the key, or another thread did.
        }

        // Store the value in this array entry.
        mint_store_64_relaxed(&m_entries[idx].value, value);
        return;
    }
}
#else
void HashTable1::SetItem(uint32_t key, uint32_t value)
{
    assert(key != 0);
    assert(value != 0);

    for (uint32_t idx = integerHash(key);; idx++)
    {
        idx &= m_arraySize - 1;

        uint32_t prevKey = mint_compare_exchange_strong_32_relaxed(&m_entries[idx].key, 0, key);
        if ((prevKey == 0) || (prevKey == key))
        {
            mint_store_32_relaxed(&m_entries[idx].value, value);
            return;
        }
    }
}
#endif

//----------------------------------------------
uint64_t HashTable1::GetItem(uint64_t key)
{
    assert(key != 0);
    for (uint64_t idx = integerHash(key);; idx++)
    {
        idx &= m_arraySize - 1;
        uint64_t probedKey = mint_load_64_relaxed(&m_entries[idx].key);
        if (probedKey == key)
            return mint_load_64_relaxed(&m_entries[idx].value);
        if (probedKey == 0)
            return 0;
    }
}


//----------------------------------------------
uint64_t HashTable1::GetItemCount()
{
    uint64_t itemCount = 0;
    for (uint64_t idx = 0; idx < m_arraySize; idx++)
    {
        if ((mint_load_64_relaxed(&m_entries[idx].key) != 0)
            && (mint_load_64_relaxed(&m_entries[idx].value) != 0))
            itemCount++;
    }
    return itemCount;
}


//----------------------------------------------
void HashTable1::Clear()
{
    memset(m_entries, 0, sizeof(Entry) * m_arraySize);
}

//----------------------------------------------
void HashTable1::DeleteItem(uint64_t key)
{
  assert(key != 0);

  for (uint64_t idx = integerHash(key);; idx++)
  {
      idx &= m_arraySize - 1;

      uint64_t probedKey = mint_load_64_relaxed(&m_entries[idx].key);
      if (probedKey == key)
      {
        mint_store_64_relaxed(&m_entries[idx].key, 0);
        mint_store_64_relaxed(&m_entries[idx].value, 0);
        return;
      }
  }
}
