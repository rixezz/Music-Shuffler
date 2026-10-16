#include "FileParser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>

using namespace std;

int loadPlaylist(const char* fname, Playlist& pl) {
    ifstream file(fname);
    if (!file.is_open()) {
        cout << "could not open " << fname << endl;
        return 0;
    }

    int loaded = 0;
    char text[300];

    while (file.getline(text, 300)) {
        if (strlen(text) == 0)
            continue;

        int number = 0;
        char title[120] = "";
        char artist[80] = "unknown";

        int field = 0;
        char* begin = text;

        for (int pos = 0; text[pos] != '\0'; pos++) {
            if (text[pos] ==  '|') {
                text[pos] = '\0';

                if (field == 0) {
                    number = atoi(begin);
                } else if (field == 1) {
                    strncpy(title, begin, 119);
                    title[119] = '\0';
                }

                field++;
                begin = &text[pos + 1];
            }
        }

        if (field == 2) {
            strncpy(artist, begin, 79);
            artist[79] = '\0';
        } else if (field == 1) {
            strncpy(title, begin, 119);
            title[119] = '\0';
        } else {
            number = loaded;
            strncpy(title, begin, 119);
            title[119] = '\0';
        }

        if (strlen(title) > 0) {
            pl.addSong(number, title, artist);
            loaded++;
        }
    }

    file.close();
    cout << "loaded " << loaded << " songs from " << fname << endl;
    return loaded;
}

