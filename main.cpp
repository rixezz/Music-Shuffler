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

Song* arr;
int n;
int now = -1;
int secs = 0;
bool paused = false;
CustomHashSet seen(128);
HistoryStack hist(500);

int getLen(int id) {
    return 170 + (id * 17) % 90;
}

void cursorOn(bool on) {
    CONSOLE_CURSOR_INFO ci;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(h, &ci);
    ci.bVisible = on;
    SetConsoleCursorInfo(h, &ci);
}

void goTop() {
    COORD c = {0, 0};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void playIdx(int i) {
    now = i;
    secs = 0;
    seen.insert(arr[now].id);
    hist.push(arr[now]);
}

void doShuffle() {
    seen.clear();
    shuffleSongs(arr, n);
    playIdx(0);
}

void goNext() {
    if (now + 1 >= n) doShuffle();
    else playIdx(now + 1);
}

void goPrev() {
    if (hist.getSize() <= 1) return;
    hist.pop();
    int id = hist.peek().id;
    for (int i = 0; i < n; i++)
        if (arr[i].id == id) now = i;
    secs = 0;
}

void row(const char* s) {
    printf("   %-70s\n", s);
}

void draw() {
    char buf[200];
    goTop();
    row("");
    row("NOW PLAYING");
    snprintf(buf, 200, "%s - %s", arr[now].title, arr[now].artist);
    row(buf);

    int len = getLen(arr[now].id);
    int bars = secs * 30 / len;
    char bar[31];
    for (int i = 0; i < 30; i++) bar[i] = (i < bars ? '#' : '.');
    bar[30] = '\0';
    snprintf(buf, 200, "%d:%02d [%s] %d:%02d  %s", secs / 60, secs % 60, bar, len / 60, len % 60, paused ? "paused" : "");
    row(buf);
    snprintf(buf, 200, "track %d of %d, played %d", now + 1, n, seen.getSize());
    row(buf);
    row("");
    row("n next | p previous | s reshuffle | space pause/play");
    row("h history | u up next | m menu");
}

void drawList(int mode) {
    char buf[200];
    goTop();
    row("");
    if (mode == 1) {
        row("HISTORY (last 10 played)");
        row("");
        for (int i = 1; i <= 10; i++) {
            Song s = hist.peekAt(i);
            if (s.id < 0) row("");
            else {
                snprintf(buf, 200, "%d. %s - %s", i, s.title, s.artist);
                row(buf);
            }
        }
    } else {
        row("UP NEXT (next 10 songs)");
        row("");
        for (int i = 1; i <= 10; i++) {
            if (now + i < n) {
                snprintf(buf, 200, "%d. %s - %s", i, arr[now + i].title, arr[now + i].artist);
                row(buf);
            } else {
                row("");
            }
        }
    }
    row("");
    row("press backspace to go back to the player");
}

void player() {
    if (now < 0) doShuffle();
    system("cls");
    cursorOn(false);
    int t = 0;
    int mode = 0;
    while (true) {
        if (mode == 0) draw();
        else drawList(mode);

        if (_kbhit()) {
            int k = _getch();
            if (mode != 0) {
                if (k == 8) mode = 0;
            }
            else if (k == 'n') goNext();
            else if (k == 'p') goPrev();
            else if (k == 's') doShuffle();
            else if (k == ' ') paused = !paused;
            else if (k == 'h') mode = 1;
            else if (k == 'u') mode = 2;
            else if (k == 'm') break;
            system("cls");
        }
        Sleep(250);
        if (paused) continue;
        t++;
        if (t % 4 == 0) {
            secs++;
            if (secs >= getLen(arr[now].id)) {
                goNext();
                system("cls");
            }
        }
    }
    cursorOn(true);
    system("cls");
}


void search() {
    char key[60], key2[60];
    cout << "\n   search: ";
    cin.getline(key, 60);
    int L = strlen(key);
    if (L == 0) return;
    for (int i = 0; i <= L; i++) key2[i] = tolower(key[i]);

    int res[200];
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        char line[220];
        snprintf(line, 220, "%s %s", arr[i].title, arr[i].artist);
        for (int k = 0; line[k]; k++) line[k] = tolower(line[k]);
        if (strstr(line, key2)) res[cnt++] = i;
    }
    if (cnt == 0) {
        cout << "   no song found\n";
        cout << "\n   press enter to go back...";
        cin.get();
        return;
    }
    for (int i = 0; i < cnt; i++)
        cout << "   " << i + 1 << ". " << arr[res[i]].title << " - " << arr[res[i]].artist << "\n";
    cout << "\n   enter number to play (0 to go back): ";
    char buf[16];
    cin.getline(buf, 16);
    int sel = atoi(buf);
    if (sel >= 1 && sel <= cnt) {
        playIdx(res[sel - 1]);
        player();
    }
}

int main() {
    system("color 0B");
    srand((unsigned)time(NULL));

    Playlist pl;
    if (loadPlaylist("playlist.txt", pl) == 0) {
        cout << "playlist.txt is missing or empty\n";
        return 1;
    }
    n = pl.getSongCount();
    arr = new Song[n];
    pl.toArray(arr, n);

    char inp[32];
    while (true) {
        system("cls");
        cout << "\n   SMART PLAYLIST SHUFFLER  (" << n << " songs)\n\n";
        if (now >= 0)
            cout << "   now playing: " << arr[now].title << " - " << arr[now].artist << "\n\n";
        cout << "   1. Live Player\n";
        cout << "   2. All Songs\n";
        cout << "   3. Search\n";
        cout << "   4. Exit\n\n";
        cout << "   Enter your choice: ";
        cin.getline(inp, 32);
        int c = atoi(inp);

        if (c == 1) player();
        else if (c == 2) {
            system("cls");
            pl.displayAll();
            cout << "   press enter to go back...";
            cin.get();
        }
        else if (c == 3) search();
        else if (c == 4) break;
    }
    delete[] arr;
    return 0;
}
