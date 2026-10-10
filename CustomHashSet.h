#ifndef CUSTOM_HASH_SET_H
#define CUSTOM_HASH_SET_H

class CustomHashSet {
public:
    explicit CustomHashSet(int initialCapacity = 32);
    ~CustomHashSet();

    CustomHashSet(const CustomHashSet& other);
    CustomHashSet& operator=(const CustomHashSet& other);

    bool insert(int id);
    bool hasPlayed(int id) const;
    void clear();

    int getSize() const;
    int getCapacity() const;
    bool isEmpty() const;

    void display() const;

private:
    static const int EMPTY = -1;
    static const int MAX_LOAD_PERCENT = 70;

    int* table;
    int capacity;
    int size;

    int hashFunction(int id) const;
    int findSlot(int id) const;
    void rehash();
};

#endif
