#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "FisherYates.h"

struct Node {
    Song song;
    Node* next;
    Node* prev;
};

class Playlist {
public:
    Playlist();
    ~Playlist();

    void addSong(int id, const char* title, const char* art);
    void removeSong(int id);
    void displayAll() const;
    int getSongCount() const;
    void toArray(Song items[], int mx) const;
    void fromArray(Song items[], int size);
    Node* getHead() const;

private:
    Node* head;
    int count;
};

#endif
