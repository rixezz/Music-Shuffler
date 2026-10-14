#ifndef PLAYLIST_PARSER_H
#define PLAYLIST_PARSER_H

struct Track
{
    int id;
    char title[100];
    char path[200];
};

class PlaylistParser
{
private:
    Track* tracks;
    int count;
    int capacity;

    bool expand();

public:
    PlaylistParser();
    ~PlaylistParser();

    bool load(const char* filename);
    bool addTrack(int id, const char* title, const char* path);
    int getCount() const;
    const Track* getTrack(int index) const;
    void clear();
};
