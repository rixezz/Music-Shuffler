#include "CustomHashSet.h"
#include <iostream>

using namespace std;

CustomHashSet::CustomHashSet(int c) {
    if (c < 4) {
        capacity = 4;
    } else {
        capacity = c;
    }

    size = 0;
    table = new int[capacity];

    for(int i=0; i < capacity; i++) {
        table[i] = EMPTY;
    }
}

CustomHashSet::~CustomHashSet() {
    if (table != nullptr) {
        delete[] table;
    }
}

CustomHashSet::CustomHashSet(const CustomHashSet& other) {
    capacity = other.capacity;
    size = other.size;
    table = new int[capacity];

    for (int i = 0; i < capacity; ++i) {
        table[i] = other.table[i];
    }
}

CustomHashSet& CustomHashSet::operator=(const CustomHashSet& other) {
    if (this == &other) {
        return *this;
    }

    delete[] table;

    capacity = other.capacity;
    size = other.size;
    table = new int[capacity];

    for (int i = 0; i < capacity; i++) {
        table[i] = other.table[i];
    }

    return *this;
}

int CustomHashSet::hashFunction(int id) const {
    return id % capacity;
}

int CustomHashSet::findSlot(int id) const {
    int pos = hashFunction(id);

    while (table[pos] != EMPTY && table[pos] != id) {
        pos = (pos + 1) % capacity;
    }

    return pos;
}

void CustomHashSet::rehash() {
    int* old = table;
    int oldCap = capacity;

    capacity = capacity * 2;
    table = new int[capacity];

    for(int i=0; i<capacity; i++) {
        table[i] = EMPTY;
    }

    for (int i = 0; i < oldCap; i++) {
        if (old[i] != EMPTY) {
            int newSpot = findSlot(old[i]);
            table[newSpot] = old[i];
        }
    }

    delete[] old;
}

bool CustomHashSet::insert(int id) {
    if (id < 0 || hasPlayed(id)) {
        return false;
    }

    if ((size + 1) * 100 > capacity * MAX_LOAD_PERCENT) {
        rehash();
    }

    int spot = findSlot(id);
    table[spot] = id;
    size++;

    return true;
}

bool CustomHashSet::hasPlayed(int id) const {
    if (id <0) return false;

    int pos2 = findSlot(id);
    return table[pos2] ==id;
}

void CustomHashSet::clear() {
    size = 0;
    for(int i = 0; i < capacity; i++) {
        table[i] = EMPTY;
    }
}

int CustomHashSet::getSize() const {
    return size;
}

int CustomHashSet::getCapacity() const {
    return capacity;
}

bool CustomHashSet::isEmpty() const {
    return size == 0;
}

void CustomHashSet::display() const {
    cout << "table info: size=" << size << ", cap=" << capacity << endl;
    for (int i = 0; i < capacity; i++) {
        if (table[i] == EMPTY) {
            cout << i << ": empty" << endl;
        } else {
            cout << i << ": " << table[i] << endl;
        }
    }
}
