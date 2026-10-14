#include "HistoryStack.h"
#include <iostream>
#include <cstring>

using namespace std;

HistoryStack::HistoryStack(int n) {
    maxN = n;
    top = -1;
    data = new Song[maxN];
}

HistoryStack::~HistoryStack() {
    delete[] data;
}

void HistoryStack::push(Song s) {
    if (top >= maxN - 1) {
        cout << "history stack is full" << endl;
        return;
    }
    top++;
    data[top] = s;
}

Song HistoryStack::pop() {
    Song b;
    b.id = -1;
    strcpy(b.title, "");
    strcpy(b.artist, "");

    if (top < 0) {
        cout << "history is empty" << endl;
        return b;
    }

    Song x = data[top];
    top--;
    return x;
}

Song HistoryStack::peek() const {
    Song b;
    b.id = -1;
    strcpy(b.title, "");
    strcpy(b.artist, "");

    if (top < 0) return b;
    return data[top];
}

Song HistoryStack::peekAt(int depth) const {
    Song b;
    b.id = -1;
    strcpy(b.title, "");
    strcpy(b.artist, "");
    if (top - depth < 0) return b;
    return data[top - depth];
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
    for (int i = top; i >= 0; i--) {
        cout << "  " << data[i].title << endl;
    }
}
