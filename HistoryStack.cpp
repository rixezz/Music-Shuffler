#include "HistoryStack.h"
#include <iostream>
#include <cstring>

using namespace std;

HistoryStack::HistoryStack(int limit) {
    maxN = limit;
    top = -1;
    data = new Song[maxN];
}

HistoryStack::~HistoryStack() {
    delete[] data;
}

void HistoryStack::push(Song item) {
    if (top >= maxN - 1) {
        for (int pos = 1; pos < maxN; pos++)
            data[pos - 1] = data[pos];
        top--;
    }
    top++;
    data[top] = item;
}

Song HistoryStack::pop() {
    Song empty;
    empty.id = -1;
    strcpy(empty.title, "");
    strcpy(empty.artist, "");

    if (top < 0) {
        cout << "history is empty" << endl;
        return empty;
    }

    Song item = data[top];
    top--;
    return item;
}

Song HistoryStack::peek() const {
    Song empty;
    empty.id = -1;
    strcpy(empty.title, "");
    strcpy(empty.artist, "");
    if (top < 0) return empty;
    return data[top];
}

Song HistoryStack::peekAt(int level) const {
    Song empty;
    empty.id = -1;
    strcpy(empty.title, "");
    strcpy(empty.artist, "");
    if (top - level < 0) return empty;
    return data[top - level];
}

bool HistoryStack::isEmpty() const {
    return top < 0;
}

int HistoryStack::getSize() const {
    return top + 1;
}

void HistoryStack::display() const {
    if (top < 0) {
        cout << "history is empty" << endl;
        return;
    }

    cout << "play history (most recent first):" << endl;
    for (int pos = top; pos >= 0; pos--) {
        cout << "  " << data[pos].title << " - " << data[pos].artist << endl;
    }
}

