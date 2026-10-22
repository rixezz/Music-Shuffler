#include "Playlist.h"
#include "FisherYates.h"
#include "CustomHashSet.h"
#include "HistoryStack.h"
#include "FileParser.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <windows.h>
#include <conio.h>

using namespace std;

Song* songList;
int total;
int current = -1;
int seconds = 0;
bool isPaused = false;
CustomHashSet playedSet(128);
HistoryStack history(500);

int getLength(int id) {
    return 170 + (id * 17) % 90;
}

void showCursor(bool visible) {
    CONSOLE_CURSOR_INFO info;
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(handle, &info);
    info.bVisible = visible;
    SetConsoleCursorInfo(handle, &info);
}

void moveToTop() {
    COORD corner = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), corner);
}

void playSong(int index) {
    current = index;
    seconds = 0;
    playedSet.insert(songList[current].id);
    history.push(songList[current]);
}

void shuffleAll() {
    playedSet.clear();
    shuffleSongs(songList, total);
    playSong(0);
}

void nextSong() {
    if (current + 1 >= total) shuffleAll();
    else playSong(current + 1);
}

void previousSong() {
    if (history.getSize() <= 1) return;
    history.pop();
    int id = history.peek().id;
    for (int counter = 0; counter < total; counter++)
        if (songList[counter].id ==id) current = counter;
    seconds = 0;
}

void printRow(const char* text) {
    printf("   %-70s\n", text);
}

void drawPlayer() {
    char line[200];
    moveToTop();
    printRow("");
    printRow("NOW PLAYING");
    snprintf(line, 200, "%s - %s", songList[current].title, songList[current].artist);
    printRow(line);

    int length = getLength(songList[current].id);
    int filled = seconds * 30 / length;
    char bar[31];
    for (int counter = 0; counter < 30; counter++) bar[counter] = (counter < filled ? '#' : '.');
    bar[30] = '\0';
    snprintf(line, 200, "%d:%02d [%s] %d:%02d  %s", seconds / 60, seconds % 60, bar, length / 60, length % 60, isPaused ? "paused" : "");
    printRow(line);
    snprintf(line, 200, "track %d of %d, played %d", current + 1, total, playedSet.getSize());
    printRow(line);
    printRow("");
    printRow("n next | p previous | s reshuffle | space pause/play");
    printRow("h history | u up next | m menu");
}

void drawList(int mode) {
    char line[200];
    moveToTop();
    printRow("");
    if (mode == 1) {
        printRow("HISTORY (last 10 played)");
        printRow("");
        for (int pos = 1; pos <= 10; pos++) {
            Song item = history.peekAt(pos);
            if (item.id < 0) printRow("");
            else {
                snprintf(line, 200, "%d. %s - %s", pos, item.title, item.artist);
                printRow(line);
            }
        }
    } else {
        printRow("UP NEXT (next 10 songs)");
        printRow("");
        for (int pos = 1; pos <= 10; pos++) {
            if (current + pos < total) {
                snprintf(line, 200, "%d. %s - %s", pos, songList[current + pos].title, songList[current + pos].artist);
                printRow(line);
            } else {
                printRow("");
            }
        }
    }
    printRow("");
    printRow("press backspace to go back to the player");
}

void livePlayer() {
    if (current < 0) shuffleAll();
    system("cls");
    showCursor(false);
    int ticks = 0;
    int mode = 0;
    while (true) {
        if (mode == 0) drawPlayer();
        else drawList(mode);

        if (_kbhit()) {
            int key = _getch();
            if (mode != 0) {
                if (key == 8) mode = 0;
            }
            else if (key == 'n') nextSong();
            else if (key == 'p') previousSong();
            else if (key == 's') shuffleAll();
            else if (key == ' ') isPaused = !isPaused;
            else if (key == 'h') mode = 1;
            else if (key == 'u') mode = 2;
            else if (key == 'm') break;
            system("cls");
        }
        Sleep(250);
        if (isPaused) continue;
        ticks++;
        if (ticks % 4 ==0) {
            seconds++;
            if (seconds >= getLength(songList[current].id)) {
                nextSong();
                system("cls");
            }
        }
    }
    showCursor(true);
    system("cls");
}

void searchSongs() {
    char word[60], lowerWord[60];
    cout << "\n   search: ";
    cin.getline(word, 60);
    int size = strlen(word);
    if (size == 0) return;
    for (int counter = 0; counter <= size; counter++) lowerWord[counter] = tolower(word[counter]);

    int matches[200];
    int found = 0;
    for (int counter = 0; counter < total; counter++) {
        char text[220];
        snprintf(text, 220, "%s %s", songList[counter].title, songList[counter].artist);
        for (int pos = 0; text[pos]; pos++) text[pos] = tolower(text[pos]);
        if (strstr(text, lowerWord)) matches[found++] = counter;
    }
    if (found == 0) {
        cout << "   no song found\n";
        cout << "\n   press enter to go back...";
        cin.get();
        return;
    }
    for (int counter = 0; counter < found; counter++)
        cout << "   " << counter + 1 << ". " << songList[matches[counter]].title << " - " << songList[matches[counter]].artist << "\n";
    cout << "\n   enter number to play (0 to go back): ";
    char input[16];
    cin.getline(input, 16);
    int choice = atoi(input);
    if (choice >= 1 && choice <= found) {
        playSong(matches[choice - 1]);
        livePlayer();
    }
}

int main() {
    system("color 0B");
    srand((unsigned)time(NULL));

    Playlist playlist;
    if (loadPlaylist("playlist.txt", playlist) == 0) {
        cout << "playlist.txt is missing or empty\n";
        return 1;
    }
    total = playlist.getSongCount();
    songList = new Song[total];
    playlist.toArray(songList, total);

    char input[32];
    while (true) {
        system("cls");
        cout << "\n   SMART PLAYLIST SHUFFLER  (" << total << " songs)\n\n";
        if (current >= 0)
            cout << "   now playing: " << songList[current].title << " - " << songList[current].artist << "\n\n";
        cout << "   1. Live Player\n";
        cout << "   2. All Songs\n";
        cout << "   3. Search\n";
        cout << "   4. Exit\n\n";
        cout << "   Enter your choice: ";
        cin.getline(input, 32);
        int choice = atoi(input);

        if (choice == 1) livePlayer();
        else if (choice == 2) {
            system("cls");
            playlist.displayAll();
            cout << "   press enter to go back...";
            cin.get();
        }
        else if (choice ==3) searchSongs();
        else if (choice == 4) break;
    }
    delete[] songList;
    return 0;
}

