#include "playlist_parser.h"
#include <fstream>
#include <cstring>
#include <cstdlib>

PlaylistParser::PlaylistParser()
{
    tracks = 0;
    count = 0;
    capacity = 0;
}

PlaylistParser::~PlaylistParser()
{
    clear();
}

bool PlaylistParser::expand()
{
    int newCapacity = (capacity == 0) ? 5 : capacity * 2;
    Track* newTracks = new Track[newCapacity];

    for (int i = 0; i < count; i++)
        newTracks[i] = tracks[i];

    delete[] tracks;

    tracks = newTracks;
    capacity = newCapacity;

    return true;
}

bool PlaylistParser::addTrack(int id, const char* title, const char* path)
{
    if (count == capacity)
        expand();

    tracks[count].id = id;

    std::strncpy(tracks[count].title, title, 99);
    tracks[count].title[99] = '\0';

    std::strncpy(tracks[count].path, path, 199);
    tracks[count].path[199] = '\0';

    count++;

    return true;
}

bool PlaylistParser::load(const char* filename)
{
    clear();

    std::ifstream file(filename);

    if (!file.is_open())
        return false;

    char line[400];

    while (file.getline(line, sizeof(line)))
    {
        if (line[0] == '\0')
            continue;

        char* first = std::strchr(line, '|');

        if (first == 0)
            continue;

        *first = '\0';

        char* second = std::strchr(first + 1, '|');

        if (second == 0)
            continue;

        *second = '\0';

        char* end;
        long id = std::strtol(line, &end, 10);

        if (line == end || *end != '\0')
            continue;

        if (id < 0 || id > 2147483647L)
            continue;

        if (*(first + 1) == '\0' || *(second + 1) == '\0')
            continue;

        addTrack(static_cast<int>(id), first + 1, second + 1);
    }

    file.close();

    return count > 0;
}

int PlaylistParser::getCount() const
{
    return count;
}

const Track* PlaylistParser::getTrack(int index) const
{
    if (index < 0 || index >= count)
        return 0;

    return &tracks[index];
}

void PlaylistParser::clear()
{
    delete[] tracks;

    tracks = 0;
    count = 0;
    capacity = 0;
}
