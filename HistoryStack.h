#ifndef HISTORY_STACK_H
#define HISTORY_STACK_H

#include "FisherYates.h"

class HistoryStack {
public:
    HistoryStack(int n = 200);
    ~HistoryStack();

    void push(Song s);
    Song pop();
    Song peek() const;
    Song peekAt(int depth) const;
    bool isEmpty() const;
    int getSize() const;
    void display() const;

private:
    Song* data;
    int top;
    int maxN;
};

#endif
