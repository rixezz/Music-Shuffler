#ifndef CUSTOM_HASH_SET_H
#define CUSTOM_HASH_SET_H

// Hash set of song IDs. The shuffler uses it to remember which songs have
// already played in the current cycle, so nothing repeats until every song
// has had a turn. No STL - just a plain int array on the heap.
//
// Collisions: open addressing with linear probing (if a slot is taken,
// try the next one). When the table would go over 70% full, it doubles.
//
// IDs must be non-negative, because -1 is used to mark an empty slot.

class CustomHashSet {
public:
    explicit CustomHashSet(int initialCapacity = 32);
    ~CustomHashSet();

    // We own a heap array, so copies need their own array (deep copy).
    CustomHashSet(const CustomHashSet& other);
    CustomHashSet& operator=(const CustomHashSet& other);

    // Adds an ID. Returns false if it's already there or if it's negative.
    bool insert(int id);

    // True if this ID was already added in the current cycle.
    bool hasPlayed(int id) const;

    // Empties the set for a new cycle. Capacity stays the same.
    void clear();

    int getSize() const;
    int getCapacity() const;
    bool isEmpty() const;

    // Prints every slot - useful for seeing where collisions ended up.
    void display() const;

private:
    static const int EMPTY = -1;
    static const int MAX_LOAD_PERCENT = 70;

    int* table;
    int capacity;
    int size;

    int hashFunction(int id) const;

    // Returns the slot that holds id, or the empty slot where id would go.
    int findSlot(int id) const;

    void rehash();
};

#endif
