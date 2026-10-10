#include "Playlist.h"
#include <iostream>
#include <cstring>

using namespace std;

Playlist::Playlist() {
    head = NULL;
    count = 0;
}

Playlist::~Playlist() {
    if (head ==NULL) return;

    Node* p = head;
    for (int pos = 0; pos < count; pos++) {
        Node* q = p->next;
        delete p;
        p = q;
    }
    head = NULL;
    count = 0;
}

void Playlist::addSong(int id, const char* title, const char* art) {
    Node* temp = new Node;
    temp->song.id = id;
    strncpy(temp->song.title, title, 119);
    temp->song.title[119] = '\0';
    strncpy(temp->song.artist, art, 79);
    temp->song.artist[79] = '\0';

    if (head ==NULL) {
        head = temp;
        head->next = head;
        head->prev = head;
    } else {
        Node* last = head->prev;
        last->next = temp;
        temp->prev = last;
        temp->next = head;
        head->prev = temp;
    }
    count++;
}

void Playlist::removeSong(int id) {
    if (head == NULL) {
        cout << "playlist is empty" << endl;
        return;
    }

    Node* p = head;
    for (int pos = 0; pos < count; pos++) {
        if (p->song.id ==id) {
            if (count == 1) {
                delete p;
                head = NULL;
            } else {
                p->prev->next = p->next;
                p->next->prev = p->prev;
                if (p ==head)
                    head = p->next;
                delete p;
            }
            count--;
            cout << "removed song id " << id << endl;
            return;
        }
        p = p->next;
    }
    cout << "song id " << id << " not found" << endl;
}

void Playlist::displayAll() const {
    if (head ==NULL) {
        cout << "no songs loaded yet" << endl;
        return;
    }

    Node* p = head;
    cout << endl;
    for (int pos = 0; pos < count; pos++) {
        cout << "  " << (pos + 1) << ". " << p->song.title << " - " << p->song.artist << endl;
        p = p->next;
    }
    cout << endl;
}

int Playlist::getSongCount() const {
    return count;
}

void Playlist::toArray(Song items[], int mx) const {
    if (head == NULL) return;

    Node* p = head;
    int lim = count;
    if (lim > mx) lim = mx;

    for (int pos = 0; pos < lim; pos++) {
        items[pos] = p->song;
        p = p->next;
    }
}

void Playlist::fromArray(Song items[], int size) {
    if (head ==NULL) return;

    Node* p = head;
    int lim = size;
    if (lim > count) lim = count;

    for (int pos = 0; pos < lim; pos++) {
        p->song = items[pos];
        p = p->next;
    }
}

Node* Playlist::getHead() const {
    return head;
}
