#include "Playlist.h"
#include "FisherYates.h"
#include "CustomHashSet.h"
#include "HistoryStack.h"
#include "FileParser.h"

#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <bcrypt.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>

#ifndef D2DERR_RECREATE_TARGET
#define D2DERR_RECREATE_TARGET ((HRESULT)0x8899000CL)
#endif

typedef D2D1_COLOR_F Col;
typedef D2D1_POINT_2F Pt;

Song* songList;
int total;
int current = -1;
int seconds = 0;
float posf = 0;
bool playing = false;
bool repeatOne = false;
CustomHashSet playedSet(128);
HistoryStack history(500);

enum { SCREEN_LOGIN, SCREEN_APP };
enum { H_NONE, H_NAV, H_SIGNOUT, H_PLAYALL, H_SHUFFLE, H_PREV, H_PLAY, H_NEXT, H_SEEK, H_ROW, H_HEART, H_SEARCH,
       H_LOGIN, H_LINK, H_BARSHUF, H_REPEAT, H_BARHEART, H_BARINFO, H_NPCLOSE, H_MENU, H_UNDO, H_USERFIELD,
       H_PASSFIELD, H_CARD, H_COUNT };
enum { F_NONE, F_USER, F_PASS, F_SEARCH };
enum { M_PLAY, M_NEXT, M_LATER, M_QUEUE, M_LIKE, M_COPY };

int screen = SCREEN_LOGIN;
bool signup = false;
char user[32] = "";
char err[100] = "";

struct Field {
    wchar_t text[64];
    int len, caret, anchor, limit;
    float scroll;
};
Field inUser = {L"", 0, 0, 0, 20, 0}, inPass = {L"", 0, 0, 0, 40, 0}, inSearch = {L"", 0, 0, 0, 50, 0};
int focusField = F_NONE;
bool caretOn = true;

HWND hMain;
int dpi = 96;
float ui = 1.0f;
#define U(x) ((x) * ui)

const int NAV_COUNT = 10;
const int NAV_MAIN = 4;   // "All Playlists"; the playlists themselves follow it
const int PL_FIRST = 5;
const int PL_COUNT = NAV_COUNT - PL_FIRST;
int view = 0;
int viewIdx[1000];
int viewCount = 0;
float scrollY = 0, scrollTarget = 0;
int hot = H_NONE;
int hotIdx = 0;
bool tracking = false;
float mouseX = -1, mouseY = -1;

bool nowView = false;
bool motion = true;
float animT = 0;

int likedIds[1000];
int likedCount = 0;
int skippedIds[1000];
int skippedCount = 0;

// animation state
float hv[H_COUNT], pv[H_COUNT], navHv[NAV_COUNT], navSel[NAV_COUNT], cardHv[PL_COUNT], rowHv[1000], menuHv[8], fieldFocus[2];
float npT = 0, viewT = 1, menuT = 0, toastAmt = 0, toastLife = 0, songT = 1, playIconT = 0, appFade = 0, loginT = 0;
float shakeT = 1, popT = 1, shiftAmt = 0, amR = 44, amG = 15, amB = 22;
int popId = -1, shiftIdx = -1, lastSongId = -1;

bool menuOpen = false;
float menuX = 0, menuY = 0;
int menuSong = -1, menuCount = 0, menuSep = 0;
int menuIds[8];
char menuLabels[8][40];

char toastText[80] = "";
bool toastUndo = false;
int toastId = -1;

int pressRow = -1, swipeRow = -1;
float pressX0 = 0, pressY0 = 0, swipeX = 0;
bool dragging = false, swipeHeld = false;
ULONGLONG lastHWheel = 0;

bool needDraw = true;
bool onlyPlayback = false;
LONGLONG lastQpc = 0, qpcFreq = 1;
bool windowAlive = true;

ID2D1Factory* d2d;
IDWriteFactory* dw;
ID2D1HwndRenderTarget* rt;
ID2D1SolidColorBrush* br;
ID2D1StrokeStyle* roundStroke;
ID2D1LinearGradientBrush* artBrush[360];
IDWriteTextFormat *tHuge, *tHero, *tLogin, *tSub, *tBody, *tBold, *tSmall, *tInitS, *tInitL, *tInitXL, *tCover;
float gAlpha = 1;

Col rgba(int r, int g, int b, int a = 255) {
    Col c = {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    return c;
}

const Col cBG = rgba(18, 18, 20);
const Col cSide = rgba(26, 26, 28);
const Col cBar = rgba(28, 28, 30);
const Col cField = rgba(44, 44, 46);
const Col cHover = rgba(31, 31, 34);
const Col cNow = rgba(40, 24, 29);
const Col cLine = rgba(44, 44, 48);
const Col cText = rgba(245, 245, 247);
const Col cMuted = rgba(152, 152, 157);
const Col cDim = rgba(170, 170, 176);
const Col cRed = rgba(232, 17, 45);
const Col cRedDark = rgba(200, 12, 38);
const Col cRedText = rgba(255, 77, 99);
const Col cErr = rgba(255, 105, 97);
const Col cWhite = rgba(255, 255, 255);

float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float easeOut(float t) { float u = 1 - clamp01(t); return 1 - u * u * u; }
float easeBack(float t) {
    float u = clamp01(t) - 1;
    return 1 + 2.70158f * u * u * u + 1.70158f * u * u;
}
Col fade(Col c, float a) { c.a *= clamp01(a); return c; }
Col mix(Col a, Col b, float t) {
    Col c = {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
    return c;
}

// moves v toward target at an exponential rate; returns true while it is still moving
bool approach(float& v, float target, float rate, float dt) {
    if (!motion) {
        bool moved = v != target;
        v = target;
        return moved;
    }
    v += (target - v) * (1 - expf(-rate * dt));
    if (fabsf(target - v) < 0.002f) { v = target; return false; }
    return true;
}

void redraw() { needDraw = true; }

struct R {
    float x, y, w, h;
    bool has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Geo {
    float W, H, sw, bh, mx, mw, mh, ly, rh;
};

R mkR(float x, float y, float w, float h) {
    R r = {x, y, w, h};
    return r;
}

Geo geo() {
    RECT rc;
    GetClientRect(hMain, &rc);
    Geo g;
    g.W = (float)rc.right;
    g.H = (float)rc.bottom;
    g.sw = U(240);
    g.bh = U(96);
    g.mx = g.sw;
    g.mw = g.W - g.sw;
    g.mh = g.H - g.bh;
    g.ly = U(176);
    g.rh = U(60);
    return g;
}

R rSearch(Geo g) { return mkR(U(16), U(64), g.sw - U(32), U(36)); }
R rNav(Geo g, int i) {
    if (i < NAV_MAIN) return mkR(U(10), U(124) + i * U(44), g.sw - U(20), U(40));
    return mkR(U(10), U(124) + NAV_MAIN * U(44) + U(44) + (i - NAV_MAIN) * U(40), g.sw - U(20), U(36));
}
const char* navName(int i, char* buf) {
    static const char* fixed[NAV_COUNT] = {"Songs", "Up Next", "History", "Favourites", "All Playlists",
                                           "Midnight Drive", "Study Music", "Phonk", "Lofi Beats", "Workout Mix"};
    snprintf(buf, 64, "%s", fixed[i]);
    return buf;
}
R rCard(Geo g, int i) {
    float cs = U(190), gap = U(26);
    int cols = (int)((g.mw - U(72) + gap) / (cs + gap));
    if (cols < 1) cols = 1;
    return mkR(g.mx + U(36) + (i % cols) * (cs + gap), U(120) + (i / cols) * (cs + U(70)), cs, cs);
}
R rSignOut(Geo g) { return mkR(U(66), g.mh - U(40), U(80), U(24)); }
R rPlayAll(Geo g) { return mkR(g.mx + U(36), U(114), U(124), U(40)); }
R rShuffle(Geo g) { return mkR(g.mx + U(172), U(114), U(124), U(40)); }
R rRow(Geo g, int i) { return mkR(g.mx + U(24), g.ly + i * g.rh - scrollY, g.mw - U(48), g.rh); }
R rRowHeart(R r) { return mkR(r.x + r.w - U(124), r.y + (r.h - U(28)) / 2, U(28), U(28)); }
R rNpClose(Geo g) { return mkR(g.mx + U(24), U(20), U(40), U(40)); }

float barCenter(Geo g) { return (U(340) + g.W - U(24)) / 2; }
float trackWidth(Geo g) {
    float room = g.W - U(24) - U(340) - U(120);
    return room < U(400) ? room : U(400);
}
R rSeek(Geo g) {
    float w = trackWidth(g);
    return mkR(barCenter(g) - w / 2, g.mh + U(60), w, U(20));
}
R rBarShuf(Geo g) { return mkR(barCenter(g) - U(140), g.mh + U(14), U(40), U(40)); }
R rPrev(Geo g) { return mkR(barCenter(g) - U(80), g.mh + U(14), U(40), U(40)); }
R rPlay(Geo g) { return mkR(barCenter(g) - U(24), g.mh + U(12), U(48), U(48)); }
R rNext(Geo g) { return mkR(barCenter(g) + U(40), g.mh + U(14), U(40), U(40)); }
R rRepeat(Geo g) { return mkR(barCenter(g) + U(100), g.mh + U(14), U(40), U(40)); }
R rBarHeart(Geo g) { return mkR(U(292), g.mh + U(30), U(32), U(32)); }
R rBarInfo(Geo g) { return mkR(U(20), g.mh + U(10), U(266), U(76)); }
R rToast(Geo g) { return mkR(g.mx + (g.mw - U(380)) / 2, g.mh - U(72), U(380), U(52)); }
R rUndo(R t) { return mkR(t.x + t.w - U(84), t.y + U(8), U(76), U(36)); }

R loginCard(Geo g) {
    float h = U(500);
    float y = (g.H - h) / 2;
    if (y < U(16)) y = U(16);
    return mkR((g.W - U(400)) / 2, y, U(400), h);
}
R loginField(R c, int i) { return mkR(c.x + U(32), c.y + U(216) + i * U(78), c.w - U(64), U(46)); }
R loginButton(R c) { return mkR(c.x + U(32), c.y + U(380), c.w - U(64), U(48)); }
R loginLink(R c) { return mkR(c.x + U(32), c.y + U(440), c.w - U(64), U(28)); }

R rMenu(Geo g) {
    float w = U(236), h = menuCount * U(36) + U(9) + U(12);
    float x = menuX, y = menuY;
    if (x + w > g.W - U(8)) x = g.W - U(8) - w;
    if (y + h > g.H - U(8)) y = g.H - U(8) - h;
    if (x < U(8)) x = U(8);
    if (y < U(8)) y = U(8);
    return mkR(x, y, w, h);
}
R rMenuItem(R m, int i) { return mkR(m.x + U(6), m.y + U(6) + i * U(36) + (i > menuSep ? U(9) : 0), m.w - U(12), U(36)); }

// ---------- accounts (offline, file based) ----------

bool derive(const char* pw, const UCHAR* salt, UCHAR* out) {
    BCRYPT_ALG_HANDLE alg;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;
    NTSTATUS st = BCryptDeriveKeyPBKDF2(alg, (PUCHAR)pw, (ULONG)strlen(pw), (PUCHAR)salt, 16, 100000, out, 32, 0);
    BCryptCloseAlgorithmProvider(alg, 0);
    return st == 0;
}

void toHex(const UCHAR* in, int n, char* out) {
    const char* digits = "0123456789abcdef";
    for (int pos = 0; pos < n; pos++) {
        out[pos * 2] = digits[in[pos] >> 4];
        out[pos * 2 + 1] = digits[in[pos] & 15];
    }
    out[n * 2] = '\0';
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool fromHex(const char* in, UCHAR* out, int n) {
    for (int pos = 0; pos < n; pos++) {
        int a = hexValue(in[pos * 2]);
        int b = hexValue(in[pos * 2 + 1]);
        if (a < 0 || b < 0) return false;
        out[pos] = (UCHAR)(a * 16 + b);
    }
    return true;
}

bool hasUsers() {
    FILE* file = fopen("users.txt", "r");
    if (!file) return false;
    char line[256];
    bool found = false;
    while (fgets(line, 256, file)) {
        if (strchr(line, '|')) found = true;
    }
    fclose(file);
    return found;
}

// fills storedName, salt and hash for a username; false when there is no such account
bool findUser(const char* name, char* storedName, UCHAR* salt, char* hashHex) {
    FILE* file = fopen("users.txt", "r");
    if (!file) return false;
    char line[256];
    bool found = false;
    while (!found && fgets(line, 256, file)) {
        char* a = strchr(line, '|');
        if (!a) continue;
        *a = '\0';
        char* b = strchr(a + 1, '|');
        if (!b) continue;
        *b = '\0';
        if (_stricmp(line, name) != 0) continue;
        char* end = strpbrk(b + 1, "\r\n");
        if (end) *end = '\0';
        if (strlen(a + 1) != 32 || strlen(b + 1) != 64) continue;
        if (!fromHex(a + 1, salt, 16)) continue;
        strncpy(storedName, line, 31);
        storedName[31] = '\0';
        strncpy(hashHex, b + 1, 64);
        hashHex[64] = '\0';
        found = true;
    }
    fclose(file);
    return found;
}

bool checkLogin(const char* name, const char* pw, char* storedName) {
    UCHAR salt[16], hash[32];
    char saved[65], now[65];
    if (!findUser(name, storedName, salt, saved)) return false;
    if (!derive(pw, salt, hash)) return false;
    toHex(hash, 32, now);
    return strcmp(saved, now) == 0;
}

bool createUser(const char* name, const char* pw) {
    UCHAR salt[16], hash[32];
    char saltHex[33], hashHex[65];
    if (BCryptGenRandom(NULL, salt, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return false;
    if (!derive(pw, salt, hash)) return false;
    toHex(salt, 16, saltHex);
    toHex(hash, 32, hashHex);
    FILE* file = fopen("users.txt", "a");
    if (!file) return false;
    fprintf(file, "%s|%s|%s\n", name, saltHex, hashHex);
    fclose(file);
    return true;
}

bool validName(const char* name) {
    int size = (int)strlen(name);
    if (size < 3 || size > 20) return false;
    for (int pos = 0; pos < size; pos++) {
        char c = name[pos];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return true;
}

// ---------- favourites (one small file per account) ----------

void favPath(char* out) {
    snprintf(out, 80, "fav_%s.txt", user);
}

bool isLiked(int id) {
    for (int counter = 0; counter < likedCount; counter++)
        if (likedIds[counter] == id) return true;
    return false;
}

void loadLikes() {
    likedCount = 0;
    char path[80];
    favPath(path);
    FILE* file = fopen(path, "r");
    if (!file) return;
    char line[40];
    while (fgets(line, 40, file) && likedCount < 1000) likedIds[likedCount++] = atoi(line);
    fclose(file);
}

void saveLikes() {
    char path[80];
    favPath(path);
    FILE* file = fopen(path, "w");
    if (!file) return;
    for (int counter = 0; counter < likedCount; counter++) fprintf(file, "%d\n", likedIds[counter]);
    fclose(file);
}

void toggleLike(int id) {
    popId = id;
    popT = 0;
    for (int counter = 0; counter < likedCount; counter++) {
        if (likedIds[counter] == id) {
            likedIds[counter] = likedIds[likedCount - 1];
            likedCount--;
            saveLikes();
            return;
        }
    }
    if (likedCount < 1000) likedIds[likedCount++] = id;
    saveLikes();
}

// ---------- queue: songs removed from the upcoming queue until the next shuffle ----------

bool isSkipped(int id) {
    for (int counter = 0; counter < skippedCount; counter++)
        if (skippedIds[counter] == id) return true;
    return false;
}

void setSkipped(int id, bool on) {
    for (int counter = 0; counter < skippedCount; counter++) {
        if (skippedIds[counter] == id) {
            if (!on) skippedIds[counter] = skippedIds[--skippedCount];
            return;
        }
    }
    if (on && skippedCount < 1000) skippedIds[skippedCount++] = id;
}

// ---------- player logic (same rules as the console version) ----------

int getLength(int id) {
    return 170 + (id * 17) % 90;
}

void fieldText(const Field& f, char* out, int size) {
    int n = WideCharToMultiByte(CP_UTF8, 0, f.text, f.len, out, size - 1, NULL, NULL);
    out[n > 0 ? n : 0] = '\0';
}

bool matches(int index, const char* word) {
    char text[220];
    snprintf(text, 220, "%s %s", songList[index].title, songList[index].artist);
    for (int pos = 0; text[pos]; pos++)
        if ((unsigned char)text[pos] < 128) text[pos] = (char)tolower((unsigned char)text[pos]);
    return strstr(text, word) != NULL;
}

int indexOfId(int id) {
    for (int counter = 0; counter < total; counter++)
        if (songList[counter].id == id) return counter;
    return -1;
}

void buildView() {
    char word[160];
    fieldText(inSearch, word, 160);
    for (int pos = 0; word[pos]; pos++)
        if ((unsigned char)word[pos] < 128) word[pos] = (char)tolower((unsigned char)word[pos]);

    viewCount = 0;
    if (view == NAV_MAIN) return;
    if (view == 2) {
        for (int level = 0; level < history.getSize() && viewCount < 1000; level++) {
            int index = indexOfId(history.peekAt(level).id);
            if (index >= 0 && matches(index, word)) viewIdx[viewCount++] = index;
        }
        return;
    }
    int from = (view == 1) ? current + 1 : 0;
    for (int pos = from; pos < total && viewCount < 1000; pos++) {
        // playlists hold the same songs, each starting from a different place
        int counter = view >= PL_FIRST ? (pos + 17 * (view - PL_FIRST)) % total : pos;
        int id = songList[counter].id;
        if (view == 1 && isSkipped(id)) continue;
        if (view == 3 && !isLiked(id)) continue;
        if (matches(counter, word)) viewIdx[viewCount++] = counter;
    }
}

int upcomingCount() {
    int count = 0;
    for (int counter = current + 1; counter < total; counter++)
        if (!isSkipped(songList[counter].id)) count++;
    return count;
}

void clampScroll() {
    Geo g = geo();
    float most = viewCount * g.rh - (g.mh - g.ly) + U(16);
    if (most < 0) most = 0;
    if (scrollTarget > most) scrollTarget = most;
    if (scrollTarget < 0) scrollTarget = 0;
    if (scrollY > most) scrollY = most;
    if (scrollY < 0) scrollY = 0;
}

void refresh() {
    buildView();
    clampScroll();
    redraw();
}

void setPos(float p) {
    posf = p;
    seconds = (int)p;
}

void playSong(int index) {
    current = index;
    setPos(0);
    playedSet.insert(songList[current].id);
    history.push(songList[current]);
}

void shuffleAll() {
    playedSet.clear();
    skippedCount = 0;
    shuffleSongs(songList, total);
    playSong(0);
    scrollY = scrollTarget = 0;
}

void nextSong() {
    int next = current + 1;
    while (next < total && isSkipped(songList[next].id)) next++;
    if (next >= total) shuffleAll();
    else playSong(next);
}

void previousSong() {
    if (posf > 3) {
        setPos(0);
        return;
    }
    if (history.getSize() <= 1) return;
    history.pop();
    int index = indexOfId(history.peek().id);
    if (index >= 0) current = index;
    setPos(0);
}

// moves one song inside the playing order; the current song is tracked by id so its index can shift
void moveSong(int from, int to) {
    Song moved = songList[from];
    if (from < to) for (int k = from; k < to; k++) songList[k] = songList[k + 1];
    else for (int k = from; k > to; k--) songList[k] = songList[k - 1];
    songList[to] = moved;
}

void playNext(int index) {
    if (index == current) return;
    int id = songList[current].id;
    setSkipped(songList[index].id, false);
    moveSong(index, index < current ? current : current + 1);
    current = indexOfId(id);
}

void playLater(int index) {
    if (index == current) return;
    int id = songList[current].id;
    setSkipped(songList[index].id, false);
    moveSong(index, total - 1);
    current = indexOfId(id);
}

void showToast(const char* text, int undoId) {
    snprintf(toastText, 80, "%s", text);
    toastUndo = undoId >= 0;
    toastId = undoId;
    toastLife = undoId >= 0 ? 5.0f : 2.2f;
}

void removeFromQueue(int index) {
    if (index <= current) return;
    setSkipped(songList[index].id, true);
    showToast("Removed from Up Next", songList[index].id);
}

void copyText(const char* s) {
    if (!OpenClipboard(hMain)) return;
    EmptyClipboard();
    size_t size = strlen(s) + 1;
    HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (handle) {
        memcpy(GlobalLock(handle), s, size);
        GlobalUnlock(handle);
        SetClipboardData(CF_TEXT, handle);
    }
    CloseClipboard();
}

// ---------- text fields (custom, so the whole window can be drawn on the GPU) ----------

Field* fieldOf(int id) {
    if (id == F_USER) return &inUser;
    if (id == F_PASS) return &inPass;
    if (id == F_SEARCH) return &inSearch;
    return NULL;
}

bool hasSelection(Field* f) { return f->anchor != f->caret; }

void deleteSelection(Field* f) {
    int a = f->anchor < f->caret ? f->anchor : f->caret;
    int b = f->anchor < f->caret ? f->caret : f->anchor;
    memmove(f->text + a, f->text + b, (f->len - b) * sizeof(wchar_t));
    f->len -= b - a;
    f->text[f->len] = L'\0';
    f->caret = f->anchor = a;
}

void insertChar(Field* f, wchar_t c) {
    if (hasSelection(f)) deleteSelection(f);
    if (f->len >= f->limit) return;
    memmove(f->text + f->caret + 1, f->text + f->caret, (f->len - f->caret) * sizeof(wchar_t));
    f->text[f->caret] = c;
    f->len++;
    f->text[f->len] = L'\0';
    f->caret = f->anchor = f->caret + 1;
}

void clearField(Field* f) {
    f->len = f->caret = f->anchor = 0;
    f->text[0] = L'\0';
    f->scroll = 0;
}

void moveCaret(Field* f, int pos, bool extend) {
    if (pos < 0) pos = 0;
    if (pos > f->len) pos = f->len;
    f->caret = pos;
    if (!extend) f->anchor = pos;
}

void pasteInto(Field* f) {
    if (!OpenClipboard(hMain)) return;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const wchar_t* data = (const wchar_t*)GlobalLock(handle);
        if (data) {
            for (int pos = 0; data[pos] && data[pos] >= 32; pos++) insertChar(f, data[pos]);
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
}

// ---------- Direct2D drawing helpers ----------

void wide(const char* s, wchar_t* out, int size) {
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, size))
        MultiByteToWideChar(CP_ACP, 0, s, -1, out, size);
}

Pt P(float x, float y) { return D2D1::Point2F(x, y); }
D2D1_ROUNDED_RECT RR(float x, float y, float w, float h, float r) {
    return D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), r, r);
}

void setBr(Col c) {
    br->SetColor(c);
    br->SetOpacity(gAlpha);
}

void drawText(const char* s, IDWriteTextFormat* f, Col c, float x, float y, float w, float h, int align = 0) {
    wchar_t buffer[260];
    wide(s, buffer, 260);
    f->SetTextAlignment(align == 0 ? DWRITE_TEXT_ALIGNMENT_LEADING : (align == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_TRAILING));
    setBr(c);
    rt->DrawText(buffer, (UINT32)wcslen(buffer), f, D2D1::RectF(x, y, x + w, y + h), br, D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void fillRound(float x, float y, float w, float h, float r, Col c) {
    setBr(c);
    rt->FillRoundedRectangle(RR(x, y, w, h, r), br);
}

void lineRound(float x, float y, float w, float h, float r, Col c, float width) {
    setBr(c);
    rt->DrawRoundedRectangle(RR(x + width / 2, y + width / 2, w - width, h - width, r), br, width);
}

void fillRect(float x, float y, float w, float h, Col c) {
    setBr(c);
    rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), br);
}

void fillOval(float x, float y, float w, float h, Col c) {
    setBr(c);
    rt->FillEllipse(D2D1::Ellipse(P(x + w / 2, y + h / 2), w / 2, h / 2), br);
}

void line(float x1, float y1, float x2, float y2, Col c, float width) {
    setBr(c);
    rt->DrawLine(P(x1, y1), P(x2, y2), br, width, roundStroke);
}

void polyline(const Pt* p, int n, Col c, float width) {
    for (int i = 0; i + 1 < n; i++) line(p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, c, width);
}

void fillPoly(const Pt* p, int n, Col c) {
    ID2D1PathGeometry* geometry = NULL;
    if (FAILED(d2d->CreatePathGeometry(&geometry))) return;
    ID2D1GeometrySink* sink = NULL;
    geometry->Open(&sink);
    sink->BeginFigure(p[0], D2D1_FIGURE_BEGIN_FILLED);
    for (int i = 1; i < n; i++) sink->AddLine(p[i]);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();
    setBr(c);
    rt->FillGeometry(geometry, br);
    geometry->Release();
}

ID2D1LinearGradientBrush* makeLinear(Pt a, Pt b, Col c1, Col c2) {
    ID2D1GradientStopCollection* stops = NULL;
    D2D1_GRADIENT_STOP gs[2] = {{0.0f, c1}, {1.0f, c2}};
    rt->CreateGradientStopCollection(gs, 2, &stops);
    ID2D1LinearGradientBrush* brush = NULL;
    if (stops) rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(a, b), stops, &brush);
    if (stops) stops->Release();
    if (brush) brush->SetOpacity(gAlpha);
    return brush;
}

void fillGradientRect(float x, float y, float w, float h, Col top, Col bottom) {
    ID2D1LinearGradientBrush* brush = makeLinear(P(0, y), P(0, y + h), top, bottom);
    if (!brush) return;
    rt->FillRectangle(D2D1::RectF(x, y, x + w, y + h), brush);
    brush->Release();
}

void glow(float cx, float cy, float rx, float ry, Col c) {
    Col edge = c;
    edge.a = 0;
    ID2D1GradientStopCollection* stops = NULL;
    D2D1_GRADIENT_STOP gs[2] = {{0.0f, c}, {1.0f, edge}};
    rt->CreateGradientStopCollection(gs, 2, &stops);
    if (!stops) return;
    ID2D1RadialGradientBrush* brush = NULL;
    rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(P(cx, cy), P(0, 0), rx, ry), stops, &brush);
    stops->Release();
    if (!brush) return;
    brush->SetOpacity(gAlpha);
    rt->FillEllipse(D2D1::Ellipse(P(cx, cy), rx, ry), brush);
    brush->Release();
}

void xform(float cx, float cy, float sx, float sy, float tx, float ty) {
    rt->SetTransform(D2D1::Matrix3x2F::Translation(-cx, -cy) * D2D1::Matrix3x2F::Scale(sx, sy) * D2D1::Matrix3x2F::Translation(cx + tx, cy + ty));
}
void xformOff() { rt->SetTransform(D2D1::Matrix3x2F::Identity()); }

void clipOn(float x, float y, float w, float h) { rt->PushAxisAlignedClip(D2D1::RectF(x, y, x + w, y + h), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE); }
void clipOff() { rt->PopAxisAlignedClip(); }

Col hsv(float h, float s, float v) {
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h / 60.0f, 2.0f) - 1));
    float m = v - c;
    float r = 0, g = 0, b = 0;
    switch ((int)(h / 60.0f)) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    Col out = {r + m, g + m, b + m, 1.0f};
    return out;
}

float channel(float c) {
    return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

// background tint taken from the playing song, kept dark enough for white and grey text
void ambientTarget(float* r, float* g, float* b) {
    if (current < 0) {
        *r = 44; *g = 15; *b = 22;
        return;
    }
    Col c = hsv((float)((songList[current].id * 47 + 13) % 360), 0.75f, 0.42f);
    float rr = c.r * 255, gg = c.g * 255, bb = c.b * 255;
    float lum = 0.2126f * channel(c.r) + 0.7152f * channel(c.g) + 0.0722f * channel(c.b);
    if (lum > 0.024f) {
        float scale = sqrtf(0.024f / lum);
        rr *= scale; gg *= scale; bb *= scale;
    }
    *r = rr; *g = gg; *b = bb;
}

ID2D1LinearGradientBrush* artGradient(int hue) {
    if (artBrush[hue]) return artBrush[hue];
    artBrush[hue] = makeLinear(P(0, 0), P(1, 1), hsv((float)hue, 0.62f, 0.95f), hsv((float)hue + 40, 0.78f, 0.50f));
    return artBrush[hue];
}

void dropGradients() {
    for (int i = 0; i < 360; i++) {
        if (artBrush[i]) artBrush[i]->Release();
        artBrush[i] = NULL;
    }
}

// every song gets its own gradient tile, since playlist.txt has no cover images
void drawArt(float x, float y, float s, const Song& song, float radius) {
    int hue = (song.id * 47 + 13) % 360;
    ID2D1LinearGradientBrush* brush = artGradient(hue);
    if (brush) {
        brush->SetStartPoint(P(x, y));
        brush->SetEndPoint(P(x + s, y + s));
        brush->SetOpacity(gAlpha);
        rt->FillRoundedRectangle(RR(x, y, s, s, radius), brush);
    }
    char initial[2] = {song.title[0] ? (char)toupper((unsigned char)song.title[0]) : '?', '\0'};
    IDWriteTextFormat* f = s > U(200) ? tInitXL : (s > U(50) ? tInitL : tInitS);
    drawText(initial, f, rgba(255, 255, 255, 215), x, y, s, s, 1);
}

// playlist covers: gradient tiles titled with the playlist name (note icon when too small for text)
void iconNote(float x, float y, float s, Col c);
void drawCover(float x, float y, float s, int p, float radius) {
    char name[64];
    navName(PL_FIRST + p, name);
    ID2D1LinearGradientBrush* brush = artGradient((p * 67 + 300) % 360);
    if (brush) {
        brush->SetStartPoint(P(x, y));
        brush->SetEndPoint(P(x + s, y + s));
        brush->SetOpacity(gAlpha);
        rt->FillRoundedRectangle(RR(x, y, s, s, radius), brush);
    }
    if (s > U(100)) drawText(name, tCover, cWhite, x + s * 0.09f, y + s * 0.08f, s * 0.82f, s * 0.5f, 0);
    else iconNote(x + s * 0.25f, y + s * 0.25f, s * 0.5f, cWhite);
}

void drawAvatar(float x, float y, float s) {
    int hue = 0;
    for (int pos = 0; user[pos]; pos++) hue = (hue * 31 + user[pos]) % 360;
    fillOval(x, y, s, s, hsv((float)hue, 0.55f, 0.75f));
    char initial[2] = {user[0] ? (char)toupper((unsigned char)user[0]) : '?', '\0'};
    drawText(initial, tBold, cWhite, x, y, s, s, 1);
}

// icons are drawn on a 20 unit grid scaled to s
void iconNote(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    fillOval(x + 3 * k, y + 12 * k, 7.5f * k, 6 * k, c);
    line(x + 9.6f * k, y + 15 * k, x + 9.6f * k, y + 3 * k, c, 1.8f * k);
    Pt flag[3] = {P(x + 9.6f * k, y + 3 * k), P(x + 16 * k, y + 5.5f * k), P(x + 15 * k, y + 10 * k)};
    polyline(flag, 3, c, 1.8f * k);
}

void iconQueue(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    line(x + 3 * k, y + 5 * k, x + 17 * k, y + 5 * k, c, 2 * k);
    line(x + 3 * k, y + 10 * k, x + 17 * k, y + 10 * k, c, 2 * k);
    line(x + 3 * k, y + 15 * k, x + 10 * k, y + 15 * k, c, 2 * k);
    Pt tri[3] = {P(x + 13 * k, y + 12 * k), P(x + 18.5f * k, y + 15 * k), P(x + 13 * k, y + 18 * k)};
    fillPoly(tri, 3, c);
}

void iconClock(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    setBr(c);
    rt->DrawEllipse(D2D1::Ellipse(P(x + 10 * k, y + 10 * k), 7.5f * k, 7.5f * k), br, 1.8f * k);
    Pt hands[3] = {P(x + 10 * k, y + 5.5f * k), P(x + 10 * k, y + 10 * k), P(x + 13.5f * k, y + 12 * k)};
    polyline(hands, 3, c, 1.8f * k);
}

void iconSearch(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    setBr(c);
    rt->DrawEllipse(D2D1::Ellipse(P(x + 8 * k, y + 8 * k), 5 * k, 5 * k), br, 1.8f * k);
    line(x + 12 * k, y + 12 * k, x + 17 * k, y + 17 * k, c, 1.8f * k);
}

void iconShuffle(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    Pt a[4] = {P(x + 2 * k, y + 6 * k), P(x + 6 * k, y + 6 * k), P(x + 12 * k, y + 14 * k), P(x + 15 * k, y + 14 * k)};
    Pt b[4] = {P(x + 2 * k, y + 14 * k), P(x + 6 * k, y + 14 * k), P(x + 12 * k, y + 6 * k), P(x + 15 * k, y + 6 * k)};
    polyline(a, 4, c, 1.8f * k);
    polyline(b, 4, c, 1.8f * k);
    Pt t1[3] = {P(x + 14 * k, y + 11 * k), P(x + 19 * k, y + 14 * k), P(x + 14 * k, y + 17 * k)};
    Pt t2[3] = {P(x + 14 * k, y + 3 * k), P(x + 19 * k, y + 6 * k), P(x + 14 * k, y + 9 * k)};
    fillPoly(t1, 3, c);
    fillPoly(t2, 3, c);
}

void iconRepeat(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    Pt a[3] = {P(x + 4 * k, y + 11 * k), P(x + 4 * k, y + 6 * k), P(x + 14 * k, y + 6 * k)};
    Pt b[3] = {P(x + 16 * k, y + 9 * k), P(x + 16 * k, y + 14 * k), P(x + 6 * k, y + 14 * k)};
    polyline(a, 3, c, 1.8f * k);
    polyline(b, 3, c, 1.8f * k);
    Pt t1[3] = {P(x + 13 * k, y + 2.5f * k), P(x + 18 * k, y + 6 * k), P(x + 13 * k, y + 9.5f * k)};
    Pt t2[3] = {P(x + 7 * k, y + 10.5f * k), P(x + 2 * k, y + 14 * k), P(x + 7 * k, y + 17.5f * k)};
    fillPoly(t1, 3, c);
    fillPoly(t2, 3, c);
}

void iconPlay(float x, float y, float s, Col c) {
    Pt tri[3] = {P(x + s * 0.2f, y + s * 0.05f), P(x + s * 0.2f, y + s * 0.95f), P(x + s * 0.92f, y + s * 0.5f)};
    fillPoly(tri, 3, c);
}

void iconPause(float x, float y, float s, Col c) {
    fillRound(x + s * 0.18f, y + s * 0.08f, s * 0.24f, s * 0.84f, s * 0.06f, c);
    fillRound(x + s * 0.58f, y + s * 0.08f, s * 0.24f, s * 0.84f, s * 0.06f, c);
}

void iconSkip(float x, float y, float s, Col c, bool forward) {
    if (forward) {
        Pt tri[3] = {P(x + s * 0.08f, y + s * 0.12f), P(x + s * 0.08f, y + s * 0.88f), P(x + s * 0.72f, y + s * 0.5f)};
        fillPoly(tri, 3, c);
        fillRound(x + s * 0.76f, y + s * 0.12f, s * 0.16f, s * 0.76f, s * 0.04f, c);
    } else {
        Pt tri[3] = {P(x + s * 0.92f, y + s * 0.12f), P(x + s * 0.92f, y + s * 0.88f), P(x + s * 0.28f, y + s * 0.5f)};
        fillPoly(tri, 3, c);
        fillRound(x + s * 0.08f, y + s * 0.12f, s * 0.16f, s * 0.76f, s * 0.04f, c);
    }
}

void iconHeart(float x, float y, float s, Col c, bool filled, float scale = 1.0f) {
    ID2D1PathGeometry* geometry = NULL;
    if (FAILED(d2d->CreatePathGeometry(&geometry))) return;
    ID2D1GeometrySink* sink = NULL;
    geometry->Open(&sink);
    sink->BeginFigure(P(x + s * 0.5f, y + s * 0.92f), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddBezier(D2D1::BezierSegment(P(x + s * 0.02f, y + s * 0.58f), P(x + s * 0.05f, y + s * 0.12f), P(x + s * 0.28f, y + s * 0.12f)));
    sink->AddBezier(D2D1::BezierSegment(P(x + s * 0.42f, y + s * 0.12f), P(x + s * 0.5f, y + s * 0.24f), P(x + s * 0.5f, y + s * 0.3f)));
    sink->AddBezier(D2D1::BezierSegment(P(x + s * 0.5f, y + s * 0.24f), P(x + s * 0.58f, y + s * 0.12f), P(x + s * 0.72f, y + s * 0.12f)));
    sink->AddBezier(D2D1::BezierSegment(P(x + s * 0.95f, y + s * 0.12f), P(x + s * 0.98f, y + s * 0.58f), P(x + s * 0.5f, y + s * 0.92f)));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    sink->Release();
    if (scale != 1.0f) xform(x + s / 2, y + s / 2, scale, scale, 0, 0);
    setBr(c);
    if (filled) rt->FillGeometry(geometry, br);
    else rt->DrawGeometry(geometry, br, s * 0.09f, roundStroke);
    if (scale != 1.0f) xformOff();
    geometry->Release();
}

void iconChevronDown(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    Pt pts[3] = {P(x + 4 * k, y + 7 * k), P(x + 10 * k, y + 13 * k), P(x + 16 * k, y + 7 * k)};
    polyline(pts, 3, c, 2 * k);
}

void iconTrash(float x, float y, float s, Col c) {
    float k = s / 20.0f;
    line(x + 3 * k, y + 5.5f * k, x + 17 * k, y + 5.5f * k, c, 1.8f * k);
    Pt handle[4] = {P(x + 7.5f * k, y + 5.5f * k), P(x + 7.5f * k, y + 3 * k), P(x + 12.5f * k, y + 3 * k), P(x + 12.5f * k, y + 5.5f * k)};
    polyline(handle, 4, c, 1.8f * k);
    lineRound(x + 5 * k, y + 7.5f * k, 10 * k, 10 * k, 2 * k, c, 1.8f * k);
    line(x + 8.5f * k, y + 10.5f * k, x + 8.5f * k, y + 14.5f * k, c, 1.6f * k);
    line(x + 11.5f * k, y + 10.5f * k, x + 11.5f * k, y + 14.5f * k, c, 1.6f * k);
}

void fmtTime(int total_seconds, char* out) {
    snprintf(out, 16, "%d:%02d", total_seconds / 60, total_seconds % 60);
}

// ---------- text field drawing ----------

void displayText(Field* f, bool password, wchar_t* out) {
    for (int i = 0; i < f->len; i++) out[i] = password ? 0x2022 : f->text[i];
    out[f->len] = L'\0';
}

IDWriteTextLayout* fieldLayout(Field* f, bool password, IDWriteTextFormat* fmt, float h) {
    wchar_t disp[80];
    displayText(f, password, disp);
    IDWriteTextLayout* layout = NULL;
    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    dw->CreateTextLayout(disp, (UINT32)f->len, fmt, 10000.0f, h, &layout);
    return layout;
}

float caretPixel(IDWriteTextLayout* layout, int pos) {
    float x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS m;
    layout->HitTestTextPosition((UINT32)pos, FALSE, &x, &y, &m);
    return x;
}

int caretFromClick(Field* f, bool password, IDWriteTextFormat* fmt, R box, float padL, float px) {
    IDWriteTextLayout* layout = fieldLayout(f, password, fmt, box.h);
    if (!layout) return f->len;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS m;
    layout->HitTestPoint(px - (box.x + padL - f->scroll), box.h / 2, &trailing, &inside, &m);
    layout->Release();
    int pos = (int)m.textPosition + (trailing ? 1 : 0);
    return pos > f->len ? f->len : pos;
}

void drawField(Field* f, bool password, R box, IDWriteTextFormat* fmt, bool focused, const char* placeholder, float padL, float padR) {
    float innerW = box.w - padL - padR;
    if (f->len == 0 && !focused) {
        drawText(placeholder, fmt, cMuted, box.x + padL, box.y, innerW, box.h, 0);
        return;
    }
    IDWriteTextLayout* layout = fieldLayout(f, password, fmt, box.h);
    if (!layout) return;
    float cx = caretPixel(layout, f->caret);
    if (cx - f->scroll > innerW - U(4)) f->scroll = cx - innerW + U(4);
    if (cx - f->scroll < 0) f->scroll = cx;
    if (f->scroll < 0) f->scroll = 0;
    clipOn(box.x + padL, box.y, innerW, box.h);
    float ox = box.x + padL - f->scroll;
    if (focused && hasSelection(f)) {
        float a = caretPixel(layout, f->anchor), b = cx;
        if (a > b) { float t = a; a = b; b = t; }
        fillRect(ox + a, box.y + U(10), b - a, box.h - U(20), fade(cRed, 0.45f));
    }
    setBr(cText);
    rt->DrawTextLayout(P(ox, box.y), layout, br, D2D1_DRAW_TEXT_OPTIONS_NONE);
    if (focused && caretOn) line(ox + cx, box.y + U(12), ox + cx, box.y + box.h - U(12), cText, U(1.5f));
    clipOff();
    layout->Release();
}

// ---------- screens ----------

void paintLogin(Geo g) {
    fillGradientRect(0, 0, g.W, g.H, rgba(44, 14, 22), rgba(14, 14, 16));
    float e = easeOut(loginT);
    gAlpha = e;
    float shake = sinf(shakeT * 36.0f) * U(9) * (1 - clamp01(shakeT));
    xform(0, 0, 1, 1, shake, (1 - e) * U(18));

    R c = loginCard(g);
    fillRound(c.x, c.y, c.w, c.h, U(20), cBar);
    lineRound(c.x, c.y, c.w, c.h, U(20), cLine, 1);

    float logo = U(64);
    float lx = c.x + (c.w - logo) / 2;
    fillRound(lx, c.y + U(36), logo, logo, U(16), cRed);
    iconNote(lx + U(14), c.y + U(36) + U(14), U(36), cWhite);

    drawText("Music Shuffler", tLogin, cText, c.x, c.y + U(116), c.w, U(38), 1);
    drawText(signup ? "Create your account" : "Sign in to continue", tBody, cMuted, c.x, c.y + U(158), c.w, U(24), 1);

    const char* labels[2] = {"Username", "Password"};
    Field* fields[2] = {&inUser, &inPass};
    for (int i = 0; i < 2; i++) {
        R f = loginField(c, i);
        drawText(labels[i], tSmall, cMuted, f.x + U(2), f.y - U(24), f.w, U(20), 0);
        fillRound(f.x, f.y, f.w, f.h, U(10), cField);
        if (fieldFocus[i] > 0.01f) lineRound(f.x, f.y, f.w, f.h, U(10), fade(cRedText, fieldFocus[i]), 2);
        drawField(fields[i], i == 1, f, tBody, focusField == F_USER + i, "", U(14), U(14));
    }

    drawText(err, tSmall, cErr, c.x + U(32), c.y + U(350), c.w - U(64), U(22), 1);

    R b = loginButton(c);
    float bs = 1 + 0.015f * hv[H_LOGIN] - 0.04f * pv[H_LOGIN];
    xform(b.x + b.w / 2, b.y + b.h / 2, bs, bs, shake, (1 - e) * U(18));
    fillRound(b.x, b.y, b.w, b.h, U(10), mix(cRed, cRedDark, hv[H_LOGIN]));
    drawText(signup ? "Create Account" : "Sign In", tBold, cWhite, b.x, b.y, b.w, b.h, 1);
    xform(0, 0, 1, 1, shake, (1 - e) * U(18));

    R l = loginLink(c);
    drawText(signup ? "Already have an account? Sign in" : "New here? Create an account", tSmall, mix(cRedText, cText, hv[H_LINK]), l.x, l.y, l.w, l.h, 1);
    xformOff();
    gAlpha = 1;
}

void drawRow(Geo g, int i) {
    R r = rRow(g, i);
    if (shiftIdx >= 0 && i >= shiftIdx) r.y += shiftAmt;
    if (r.y + r.h < g.ly || r.y > g.mh) return;

    int visible = i - (int)(scrollY / g.rh);
    if (visible < 0) visible = 0;
    float e = viewT >= 0.999f ? 1.0f : easeOut(clamp01(viewT * 1.9f - visible * 0.05f));
    float lead = gAlpha;
    gAlpha = e * lead;
    float dy = (1 - e) * U(14);
    r.y += dy;

    int index = viewIdx[i];
    bool now = index == current;
    bool pinned = menuOpen && menuSong == index;
    bool swiping = swipeRow == i;
    bool skipped = isSkipped(songList[index].id);
    float sx = swiping ? swipeX : 0;

    if (swiping && sx < -1) {
        float amount = clamp01(-sx / U(110));
        fillRound(r.x, r.y + U(2), r.w, r.h - U(4), U(10), mix(rgba(60, 14, 22), cRed, amount));
        float tx = r.x + r.w - U(120);
        iconTrash(tx, r.y + (r.h - U(22)) / 2, U(22), fade(cWhite, amount));
        drawText("Remove", tBold, fade(cWhite, amount), tx + U(30), r.y, U(80), r.h, 0);
    }

    xform(0, 0, 1, 1, sx, 0);
    if (now) fillRound(r.x, r.y + U(2), r.w, r.h - U(4), U(10), cNow);
    float over = fmaxf(rowHv[i], pinned ? 1.0f : 0.0f);
    if (!now && over > 0.01f) fillRound(r.x, r.y + U(2), r.w, r.h - U(4), U(10), fade(cHover, over));
    if (swiping) fillRound(r.x, r.y + U(2), r.w, r.h - U(4), U(10), cHover);

    float ax = r.x + U(12), ay = r.y + (r.h - U(44)) / 2;
    drawArt(ax, ay, U(44), songList[index], U(6));
    if (now) {
        fillRound(ax, ay, U(44), U(44), U(6), rgba(0, 0, 0, 150));
        for (int bar = 0; bar < 3; bar++) {
            float wave = 0.5f + 0.5f * sinf(animT * 6.0f + bar * 1.7f);
            float bh = playing && motion ? U(6) + U(14) * wave : U(12);
            fillRound(ax + U(13) + bar * U(7), ay + U(22) + U(8) - bh, U(4), bh, U(2), cWhite);
        }
    }

    bool played = view != 2 && !now && playedSet.hasPlayed(songList[index].id);
    Col titleColor = now ? cRedText : ((played || skipped) ? cDim : cText);
    drawText(songList[index].title, tBody, titleColor, r.x + U(70), r.y + U(8), r.w - U(210), U(24), 0);
    if (skipped) drawText("Removed from Up Next", tSmall, cMuted, r.x + U(70), r.y + U(31), r.w - U(210), U(20), 0);
    else drawText(songList[index].artist, tSmall, cMuted, r.x + U(70), r.y + U(31), r.w - U(210), U(20), 0);

    bool liked = isLiked(songList[index].id);
    R hr = rRowHeart(r);
    float pop = (popId == songList[index].id) ? 1 + 0.45f * sinf(3.14159f * popT) : 1.0f;
    float heartHover = (hot == H_HEART && hotIdx == i) ? 1.0f : 0.0f;
    if (liked) iconHeart(hr.x + U(5), hr.y + U(5), U(18), cRedText, true, pop);
    else if (over > 0.05f) iconHeart(hr.x + U(5), hr.y + U(5), U(18), fade(heartHover > 0 ? cText : cMuted, over), false, pop);

    char tm[16];
    fmtTime(getLength(songList[index].id), tm);
    drawText(tm, tSmall, cMuted, r.x + r.w - U(80), r.y, U(66), r.h, 2);
    if (!now && over < 0.05f) fillRect(r.x + U(70), r.y + r.h - 1, r.w - U(80), 1, cLine);
    xformOff();
    gAlpha = lead;
}

void paintLists(Geo g) {
    char title[64];
    navName(view, title);
    float he = easeOut(viewT);
    float lead = gAlpha;
    gAlpha = he * lead;
    drawText(title, tHero, cText, g.mx + U(36) + (1 - he) * U(10), U(24), g.mw - U(72), U(50), 0);
    char text[120];
    if (view == 0) snprintf(text, 120, "%d songs  \xB7  %d played", total, playedSet.getSize());
    else if (view == 1) snprintf(text, 120, upcomingCount() == 1 ? "1 song coming up" : "%d songs coming up", upcomingCount());
    else if (view == 2) snprintf(text, 120, "%d recently played", history.getSize());
    else if (view == NAV_MAIN) snprintf(text, 120, "%d playlists", PL_COUNT);
    else if (view >= PL_FIRST) snprintf(text, 120, "Playlist  \xB7  %d songs", total);
    else snprintf(text, 120, likedCount == 1 ? "1 song you love" : "%d songs you love", likedCount);
    drawText(text, tBody, cDim, g.mx + U(38), U(76), g.mw - U(76), U(24), 0);
    gAlpha = lead;

    if (view == NAV_MAIN) {
        for (int p = 0; p < PL_COUNT; p++) {
            R c = rCard(g, p);
            float ce = easeOut(clamp01(viewT * 1.9f - p * 0.07f));
            gAlpha = ce * lead;
            float dy = (1 - ce) * U(16);
            float cs = 1 + 0.03f * cardHv[p];
            xform(c.x + c.w / 2, c.y + c.h / 2 + dy, cs, cs, 0, dy);
            for (int layer = 0; layer < 4; layer++)
                fillRound(c.x - layer * U(2), c.y + U(6) - layer, c.w + layer * U(4), c.h + layer * U(3), U(12) + layer, rgba(0, 0, 0, 22));
            drawCover(c.x, c.y, c.w, p, U(10));
            xformOff();
            char name[64];
            drawText(navName(PL_FIRST + p, name), tBold, cText, c.x, c.y + c.h + dy + U(6), c.w, U(24), 0);
            char count[32];
            snprintf(count, 32, "%d songs", total);
            drawText(count, tSmall, cDim, c.x, c.y + c.h + dy + U(28), c.w, U(20), 0);
        }
        gAlpha = lead;
        return;
    }

    R pa = rPlayAll(g);
    float ps = 1 + 0.02f * hv[H_PLAYALL] - 0.05f * pv[H_PLAYALL];
    xform(pa.x + pa.w / 2, pa.y + pa.h / 2, ps, ps, 0, 0);
    fillRound(pa.x, pa.y, pa.w, pa.h, U(10), mix(cRed, cRedDark, hv[H_PLAYALL]));
    iconPlay(pa.x + U(24), pa.y + (pa.h - U(16)) / 2, U(16), cWhite);
    drawText("Play", tBold, cWhite, pa.x + U(50), pa.y, pa.w - U(50), pa.h, 0);
    xformOff();

    R sh = rShuffle(g);
    float ss = 1 + 0.02f * hv[H_SHUFFLE] - 0.05f * pv[H_SHUFFLE];
    xform(sh.x + sh.w / 2, sh.y + sh.h / 2, ss, ss, 0, 0);
    fillRound(sh.x, sh.y, sh.w, sh.h, U(10), mix(cField, cLine, hv[H_SHUFFLE]));
    iconShuffle(sh.x + U(18), sh.y + (sh.h - U(20)) / 2, U(20), cRedText);
    drawText("Shuffle", tBold, cText, sh.x + U(44), sh.y, sh.w - U(44), sh.h, 0);
    xformOff();

    clipOn(g.mx, g.ly, g.mw, g.mh - g.ly);
    if (viewCount == 0) {
        const char* msg = "Nothing here yet.";
        if (inSearch.len > 0) msg = "No results.";
        else if (view == 1) msg = "The queue is empty. Press Shuffle to start a new one.";
        else if (view == 2) msg = "Songs you play will show up here.";
        else if (view == 3) msg = "Tap the heart on a song to add it here.";
        gAlpha = he * lead;
        drawText(msg, tBody, cMuted, g.mx, g.ly + U(40), g.mw, U(30), 1);
        gAlpha = lead;
    }
    for (int i = 0; i < viewCount; i++) {
        float keep = gAlpha;
        drawRow(g, i);
        gAlpha = keep;
    }
    clipOff();

    float most = viewCount * g.rh - (g.mh - g.ly) + U(16);
    if (most > 0) {
        float area = g.mh - g.ly - U(8);
        float thumb = area * area / (area + most);
        if (thumb < U(30)) thumb = U(30);
        float top = g.ly + (area - thumb) * (scrollY / most);
        fillRound(g.W - U(9), top, U(4), thumb, U(2), rgba(255, 255, 255, 70));
    }
}

void paintNow(Geo g) {
    const Song& song = songList[current];
    int hue = (song.id * 47 + 13) % 360;
    float e = easeOut(npT);
    float asz = fminf(U(320), fminf(g.mw * 0.36f, g.mh - U(140)));
    float tw = fminf(U(480), g.mw - asz - U(160));
    if (tw < U(180)) tw = U(180);
    float ax = g.mx + (g.mw - (asz + U(48) + tw)) / 2, ay = (g.mh - asz) / 2 + U(10);

    gAlpha = e;
    clipOn(g.mx, 0, g.mw, g.mh);
    Col gc = hsv((float)hue, 0.7f, 0.9f);
    gc.a = 0.5f;
    glow(ax + asz / 2, ay + asz / 2, asz * 1.3f, asz * 1.3f, gc);
    clipOff();

    float breathe = playing && motion ? 1 + 0.006f * sinf(animT * 1.3f) : 1.0f;
    float scale = (0.9f + 0.1f * e) * breathe;
    xform(ax + asz / 2, ay + asz / 2, scale, scale, 0, (1 - e) * U(24));
    for (int layer = 0; layer < 8; layer++)
        fillRound(ax + U(10) - layer * U(3), ay + U(24) - layer * U(2), asz - U(20) + layer * U(6), asz - U(8) + layer * U(5), U(18) + layer * 2, rgba(0, 0, 0, 16));
    drawArt(ax, ay, asz, song, U(14));
    xformOff();

    float tx = ax + asz + U(48);
    float t1 = easeOut(npT * 1.5f - 0.2f), t2 = easeOut(npT * 1.5f - 0.35f), t3 = easeOut(npT * 1.5f - 0.5f);
    gAlpha = t1;
    drawText(song.title, tHuge, cText, tx, ay + U(4) + (1 - t1) * U(14), tw, U(60), 0);
    gAlpha = t2;
    drawText(song.artist, tSub, cDim, tx, ay + U(66) + (1 - t2) * U(14), tw, U(32), 0);
    char meta[80];
    snprintf(meta, 80, "Track %d of %d", current + 1, total);
    drawText(meta, tSmall, cDim, tx, ay + U(100) + (1 - t2) * U(14), tw, U(22), 0);

    gAlpha = t3;
    drawText("Up Next", tBold, cText, tx, ay + U(150), tw, U(26), 0);
    int shown = 0;
    for (int index = current + 1; index < total && shown < 3; index++) {
        if (isSkipped(songList[index].id)) continue;
        float y = ay + U(184) + shown * U(54);
        if (y + U(44) > g.mh - U(8)) break;
        drawArt(tx, y, U(44), songList[index], U(6));
        drawText(songList[index].title, tBody, cText, tx + U(58), y, tw - U(58), U(24), 0);
        drawText(songList[index].artist, tSmall, cDim, tx + U(58), y + U(22), tw - U(58), U(20), 0);
        shown++;
    }
    gAlpha = e;

    R c = rNpClose(g);
    fillOval(c.x, c.y, c.w, c.h, mix(rgba(26, 26, 28, 170), cField, hv[H_NPCLOSE]));
    iconChevronDown(c.x + U(10), c.y + U(10), U(20), cText);
    gAlpha = 1;
}

void paintMenu(Geo g) {
    R m = rMenu(g);
    float e = easeBack(menuT);
    float a = clamp01(menuT * 1.6f);
    float s = 0.9f + 0.1f * e;
    xform(m.x, m.y, s, s, 0, 0);
    gAlpha = a;
    for (int layer = 0; layer < 4; layer++)
        fillRound(m.x - layer * U(2), m.y + U(6) - layer * U(1), m.w + layer * U(4), m.h + layer * U(3), U(14) + layer, rgba(0, 0, 0, 26));
    fillRound(m.x, m.y, m.w, m.h, U(12), rgba(38, 38, 41));
    lineRound(m.x, m.y, m.w, m.h, U(12), rgba(66, 66, 71), 1);
    for (int i = 0; i < menuCount; i++) {
        R it = rMenuItem(m, i);
        if (menuHv[i] > 0.01f) fillRound(it.x, it.y, it.w, it.h, U(8), fade(cRed, menuHv[i]));
        drawText(menuLabels[i], tBody, cText, it.x + U(14), it.y, it.w - U(20), it.h, 0);
        if (i == menuSep) fillRect(m.x + U(14), it.y + it.h + U(4), m.w - U(28), 1, rgba(66, 66, 71));
    }
    gAlpha = 1;
    xformOff();
}

void paintToast(Geo g) {
    if (toastAmt < 0.01f) return;
    R t = rToast(g);
    float e = easeOut(toastAmt);
    gAlpha = e;
    float dy = (1 - e) * U(22);
    for (int layer = 0; layer < 3; layer++)
        fillRound(t.x - layer * U(2), t.y + dy + U(5) - layer, t.w + layer * U(4), t.h + layer * U(3), U(16) + layer, rgba(0, 0, 0, 28));
    fillRound(t.x, t.y + dy, t.w, t.h, U(14), rgba(44, 44, 47));
    lineRound(t.x, t.y + dy, t.w, t.h, U(14), rgba(72, 72, 78), 1);
    drawText(toastText, tBody, cText, t.x + U(18), t.y + dy, t.w - (toastUndo ? U(110) : U(36)), t.h, 0);
    if (toastUndo) {
        R u = rUndo(t);
        fillRound(u.x, u.y + dy, u.w, u.h, U(9), fade(cRed, 0.18f + 0.45f * hv[H_UNDO]));
        drawText("Undo", tBold, mix(cRedText, cWhite, hv[H_UNDO]), u.x, u.y + dy, u.w, u.h, 1);
    }
    gAlpha = 1;
}

void paintApp(Geo g) {
    Col top = rgba((int)amR, (int)amG, (int)amB);
    clipOn(g.mx, 0, g.mw, g.mh);
    fillGradientRect(g.mx, 0, g.mw, U(460), top, cBG);
    Col drift = rgba((int)fminf(amR * 2.2f, 255), (int)fminf(amG * 2.2f, 255), (int)fminf(amB * 2.2f, 255), 38);
    glow(g.mx + g.mw * (0.78f + 0.05f * sinf(animT * 0.22f)), U(30), g.mw * 0.55f, U(300), drift);
    clipOff();

    // sidebar
    fillRect(0, 0, g.sw, g.mh, cSide);
    fillRect(g.sw - 1, 0, 1, g.mh, cLine);
    fillRound(U(18), U(18), U(30), U(30), U(8), cRed);
    iconNote(U(18) + U(5), U(18) + U(5), U(20), cWhite);
    drawText("Shuffler", tBold, cText, U(58), U(18), g.sw - U(70), U(30), 0);

    R s = rSearch(g);
    fillRound(s.x, s.y, s.w, s.h, U(10), cField);
    if (focusField == F_SEARCH) lineRound(s.x, s.y, s.w, s.h, U(10), cRedText, 2);
    iconSearch(s.x + U(10), s.y + (s.h - U(18)) / 2, U(18), cMuted);
    drawField(&inSearch, false, s, tBody, focusField == F_SEARCH, "Search", U(36), U(10));

    drawText("Playlists", tBold, cText, rNav(g, NAV_MAIN).x + U(12), rNav(g, NAV_MAIN).y - U(34), g.sw - U(40), U(24), 0);
    for (int i = 0; i < NAV_COUNT; i++) {
        R n = rNav(g, i);
        if (navHv[i] > 0.01f) fillRound(n.x, n.y, n.w, n.h, U(10), fade(cHover, navHv[i]));
        if (navSel[i] > 0.01f) fillRound(n.x, n.y, n.w, n.h, U(10), fade(cField, navSel[i]));
        float iy = n.y + (n.h - U(20)) / 2;
        if (i == 0) iconNote(n.x + U(12), iy, U(20), cRedText);
        else if (i == 1) iconQueue(n.x + U(12), iy, U(20), cRedText);
        else if (i == 2) iconClock(n.x + U(12), iy, U(20), cRedText);
        else if (i == 3) iconHeart(n.x + U(12), iy, U(20), cRedText, false);
        else if (i == NAV_MAIN) iconQueue(n.x + U(12), iy, U(20), cRedText);
        else drawCover(n.x + U(12), n.y + (n.h - U(28)) / 2, U(28), i - PL_FIRST, U(5));
        char label[64];
        drawText(navName(i, label), tBody, cText, n.x + U(46), n.y, n.w - U(52), n.h, 0);
    }

    float chipY = g.mh - U(68);
    fillRect(U(16), chipY - U(8), g.sw - U(32), 1, cLine);
    drawAvatar(U(18), chipY + U(4), U(40));
    drawText(user, tBold, cText, U(68), chipY + U(2), g.sw - U(80), U(24), 0);
    R so = rSignOut(g);
    drawText("Sign out", tSmall, mix(cRedText, cText, hv[H_SIGNOUT]), so.x + U(2), so.y - U(10), so.w, so.h, 0);

    if (npT > 0.005f) paintNow(g);
    if (npT < 0.995f) {
        gAlpha = 1 - easeOut(npT);
        paintLists(g);
        gAlpha = 1;
    }

    paintToast(g);

    // now playing bar
    float bt = g.mh;
    fillRect(0, bt, g.W, g.bh, cBar);
    fillRect(0, bt, g.W, 1, cLine);
    const Song& song = songList[current];
    R info = rBarInfo(g);
    if (hv[H_BARINFO] > 0.01f) fillRound(info.x - U(6), info.y, info.w + U(6), info.h, U(10), fade(cHover, hv[H_BARINFO]));
    float se = easeOut(songT);
    float as = 0.9f + 0.1f * se;
    xform(U(20) + U(28), bt + g.bh / 2, as, as, 0, 0);
    drawArt(U(20), bt + (g.bh - U(56)) / 2, U(56), song, U(8));
    xformOff();
    gAlpha = se;
    drawText(song.title, tBold, cText, U(90) + (1 - se) * U(12), bt + U(24), U(196), U(24), 0);
    drawText(song.artist, tSmall, cMuted, U(90) + (1 - se) * U(12), bt + U(48), U(196), U(22), 0);
    gAlpha = 1;
    R bh = rBarHeart(g);
    bool likedNow = isLiked(song.id);
    float pop = (popId == song.id) ? 1 + 0.45f * sinf(3.14159f * popT) : 1.0f;
    iconHeart(bh.x + U(6), bh.y + U(6), U(20), likedNow ? cRedText : mix(cMuted, cText, hv[H_BARHEART]), likedNow, pop);

    R bs = rBarShuf(g), pv2 = rPrev(g), pl = rPlay(g), nx = rNext(g), rp = rRepeat(g);
    if (hv[H_BARSHUF] > 0.01f) fillOval(bs.x, bs.y, bs.w, bs.h, fade(cHover, hv[H_BARSHUF]));
    if (hv[H_PREV] > 0.01f) fillOval(pv2.x, pv2.y, pv2.w, pv2.h, fade(cHover, hv[H_PREV]));
    if (hv[H_NEXT] > 0.01f) fillOval(nx.x, nx.y, nx.w, nx.h, fade(cHover, hv[H_NEXT]));
    if (hv[H_REPEAT] > 0.01f) fillOval(rp.x, rp.y, rp.w, rp.h, fade(cHover, hv[H_REPEAT]));
    float sb = 1 - 0.1f * pv[H_BARSHUF], sp = 1 - 0.1f * pv[H_PREV], sn = 1 - 0.1f * pv[H_NEXT], sr = 1 - 0.1f * pv[H_REPEAT];
    xform(bs.x + U(20), bs.y + U(20), sb, sb, 0, 0); iconShuffle(bs.x + U(10), bs.y + U(10), U(20), cMuted); xformOff();
    xform(pv2.x + U(20), pv2.y + U(20), sp, sp, 0, 0); iconSkip(pv2.x + U(9), pv2.y + U(9), U(22), cText, false); xformOff();
    xform(nx.x + U(20), nx.y + U(20), sn, sn, 0, 0); iconSkip(nx.x + U(9), nx.y + U(9), U(22), cText, true); xformOff();
    xform(rp.x + U(20), rp.y + U(20), sr, sr, 0, 0); iconRepeat(rp.x + U(10), rp.y + U(10), U(20), repeatOne ? cRedText : cMuted); xformOff();

    float pls = 1 + 0.05f * hv[H_PLAY] - 0.08f * pv[H_PLAY];
    xform(pl.x + pl.w / 2, pl.y + pl.h / 2, pls, pls, 0, 0);
    fillOval(pl.x, pl.y, pl.w, pl.h, mix(cRed, cRedDark, hv[H_PLAY]));
    gAlpha = 1 - playIconT;
    float iconScale = 0.8f + 0.2f * (1 - playIconT);
    xform(pl.x + pl.w / 2, pl.y + pl.h / 2, pls * iconScale, pls * iconScale, 0, 0);
    iconPlay(pl.x + U(13), pl.y + U(13), U(22), cWhite);
    gAlpha = playIconT;
    iconScale = 0.8f + 0.2f * playIconT;
    xform(pl.x + pl.w / 2, pl.y + pl.h / 2, pls * iconScale, pls * iconScale, 0, 0);
    iconPause(pl.x + U(13), pl.y + U(13), U(22), cWhite);
    gAlpha = 1;
    xformOff();

    R sk = rSeek(g);
    int length = getLength(song.id);
    float th = U(4) + U(2) * hv[H_SEEK];
    float ty = sk.y + (sk.h - th) / 2;
    fillRound(sk.x, ty, sk.w, th, th / 2, rgba(72, 72, 76));
    float done = sk.w * posf / length;
    if (done > sk.w) done = sk.w;
    if (done > 0) fillRound(sk.x, ty, done, th, th / 2, cText);
    if (hv[H_SEEK] > 0.02f) fillOval(sk.x + done - U(6) * hv[H_SEEK], sk.y + sk.h / 2 - U(6) * hv[H_SEEK], U(12) * hv[H_SEEK], U(12) * hv[H_SEEK], cText);
    char t1[16], t2[16];
    fmtTime(seconds, t1);
    fmtTime(length, t2);
    drawText(t1, tSmall, cMuted, sk.x - U(56), sk.y, U(48), sk.h, 2);
    drawText(t2, tSmall, cMuted, sk.x + sk.w + U(8), sk.y, U(48), sk.h, 0);

    if (menuOpen) paintMenu(g);
    if (appFade < 0.999f) fillRect(0, 0, g.W, g.H, fade(cBG, 1 - appFade));
}

void paintAll() {
    Geo g = geo();
    fillRect(0, 0, g.W, g.H, cBG);
    if (screen == SCREEN_LOGIN) paintLogin(g);
    else paintApp(g);
}

// ---------- device resources and frame ----------

void destroyTarget() {
    dropGradients();
    if (br) br->Release();
    if (rt) rt->Release();
    br = NULL;
    rt = NULL;
}

bool createTarget() {
    RECT rc;
    GetClientRect(hMain, &rc);
    D2D1_RENDER_TARGET_PROPERTIES rp = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(), 96.0f, 96.0f);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hp = D2D1::HwndRenderTargetProperties(hMain, D2D1::SizeU(rc.right, rc.bottom));
    if (FAILED(d2d->CreateHwndRenderTarget(rp, hp, &rt))) return false;
    rt->CreateSolidColorBrush(cWhite, &br);
    rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    return br != NULL;
}

void render() {
    if (!rt && !createTarget()) return;
    rt->BeginDraw();
    rt->SetTransform(D2D1::Matrix3x2F::Identity());
    gAlpha = 1;
    paintAll();
    HRESULT hr = rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        destroyTarget();
        redraw();
    }
}

// ---------- input ----------

int hitTest(float x, float y) {
    Geo g = geo();
    hotIdx = 0;
    if (screen == SCREEN_LOGIN) {
        R c = loginCard(g);
        if (loginField(c, 0).has(x, y)) return H_USERFIELD;
        if (loginField(c, 1).has(x, y)) return H_PASSFIELD;
        if (loginButton(c).has(x, y)) return H_LOGIN;
        if (loginLink(c).has(x, y)) return H_LINK;
        return H_NONE;
    }
    if (menuOpen) {
        R m = rMenu(g);
        if (m.has(x, y)) {
            hotIdx = -1;
            for (int i = 0; i < menuCount; i++)
                if (rMenuItem(m, i).has(x, y)) hotIdx = i;
            return H_MENU;
        }
    }
    if (toastAmt > 0.5f && toastUndo && rUndo(rToast(g)).has(x, y)) return H_UNDO;
    if (y >= g.mh) {
        if (rPlay(g).has(x, y)) return H_PLAY;
        if (rPrev(g).has(x, y)) return H_PREV;
        if (rNext(g).has(x, y)) return H_NEXT;
        if (rBarShuf(g).has(x, y)) return H_BARSHUF;
        if (rRepeat(g).has(x, y)) return H_REPEAT;
        if (rSeek(g).has(x, y)) return H_SEEK;
        if (rBarHeart(g).has(x, y)) return H_BARHEART;
        if (rBarInfo(g).has(x, y)) return H_BARINFO;
        return H_NONE;
    }
    if (x < g.sw) {
        for (int i = 0; i < NAV_COUNT; i++)
            if (rNav(g, i).has(x, y)) { hotIdx = i; return H_NAV; }
        if (rSearch(g).has(x, y)) return H_SEARCH;
        R so = rSignOut(g);
        if (mkR(so.x, so.y - U(12), so.w, so.h).has(x, y)) return H_SIGNOUT;
        return H_NONE;
    }
    if (nowView) return rNpClose(g).has(x, y) ? H_NPCLOSE : H_NONE;
    if (view == NAV_MAIN) {
        for (int p = 0; p < PL_COUNT; p++)
            if (rCard(g, p).has(x, y)) { hotIdx = p; return H_CARD; }
        return H_NONE;
    }
    if (rPlayAll(g).has(x, y)) return H_PLAYALL;
    if (rShuffle(g).has(x, y)) return H_SHUFFLE;
    if (y >= g.ly) {
        int i = (int)floorf((y - g.ly + scrollY) / g.rh);
        if (i >= 0 && i < viewCount) {
            hotIdx = i;
            return rRowHeart(rRow(g, i)).has(x, y) ? H_HEART : H_ROW;
        }
    }
    return H_NONE;
}

void closeMenu() {
    menuOpen = false;
    menuT = 0;
}

void focusOn(int id) {
    focusField = id;
    caretOn = true;
    redraw();
}

void enterApp(const char* name) {
    strncpy(user, name, 31);
    user[31] = '\0';
    clearField(&inUser);
    clearField(&inPass);
    clearField(&inSearch);
    err[0] = '\0';
    screen = SCREEN_APP;
    view = 0;
    scrollY = scrollTarget = 0;
    nowView = false;
    npT = 0;
    viewT = 0;
    appFade = 0;
    focusField = F_NONE;
    closeMenu();
    loadLikes();
    ambientTarget(&amR, &amG, &amB);
    lastSongId = -1;
    refresh();
}

void signOut() {
    playing = false;
    screen = SCREEN_LOGIN;
    signup = !hasUsers();
    err[0] = '\0';
    loginT = 0;
    closeMenu();
    focusOn(F_USER);
}

void submit() {
    char name[64], pw[96], stored[32];
    fieldText(inUser, name, 64);
    fieldText(inPass, pw, 96);
    err[0] = '\0';
    if (!signup) {
        if (!name[0] || !pw[0]) snprintf(err, 100, "Enter your username and password.");
        else if (!checkLogin(name, pw, stored)) snprintf(err, 100, "Incorrect username or password.");
        else { enterApp(stored); return; }
    } else {
        char probe[32], hash[65];
        UCHAR salt[16];
        if (!validName(name)) snprintf(err, 100, "Username: 3 to 20 letters, numbers, . _ -");
        else if (strlen(pw) < 6) snprintf(err, 100, "Password needs at least 6 characters.");
        else if (findUser(name, probe, salt, hash)) snprintf(err, 100, "That username is already taken.");
        else if (!createUser(name, pw)) snprintf(err, 100, "Could not save the account.");
        else { enterApp(name); return; }
    }
    shakeT = 0;
    redraw();
}

void runMenu(int item) {
    int index = menuSong;
    char name[260];
    switch (menuIds[item]) {
    case M_PLAY: playSong(index); playing = true; break;
    case M_NEXT: playNext(index); showToast("Playing next", -1); break;
    case M_LATER: playLater(index); showToast("Added to the end of the queue", -1); break;
    case M_QUEUE:
        if (isSkipped(songList[index].id)) { setSkipped(songList[index].id, false); showToast("Added back to Up Next", -1); }
        else removeFromQueue(index);
        break;
    case M_LIKE:
        toggleLike(songList[index].id);
        showToast(isLiked(songList[index].id) ? "Added to Favourites" : "Removed from Favourites", -1);
        break;
    case M_COPY:
        snprintf(name, 260, "%s - %s", songList[index].title, songList[index].artist);
        copyText(name);
        showToast("Copied to clipboard", -1);
        break;
    }
}

bool swipeable(int viewRow) {
    int index = viewIdx[viewRow];
    return (view == 0 || view == 1) && index > current && !isSkipped(songList[index].id);
}

void click(float x, float y) {
    int target = hitTest(x, y);
    int idx = hotIdx;
    if (menuOpen) {
        if (target == H_MENU && idx >= 0) runMenu(idx);
        closeMenu();
        refresh();
        return;
    }
    if (target != H_SEARCH && target != H_USERFIELD && target != H_PASSFIELD) focusField = F_NONE;
    if (target > H_NONE && target < H_COUNT) pv[target] = 1;
    Geo g = geo();
    switch (target) {
    case H_USERFIELD:
    case H_PASSFIELD: {
        Field* f = target == H_USERFIELD ? &inUser : &inPass;
        R box = loginField(loginCard(g), target == H_USERFIELD ? 0 : 1);
        moveCaret(f, caretFromClick(f, target == H_PASSFIELD, tBody, box, U(14), x), false);
        focusOn(target == H_USERFIELD ? F_USER : F_PASS);
        return;
    }
    case H_SEARCH: {
        R box = rSearch(g);
        moveCaret(&inSearch, caretFromClick(&inSearch, false, tBody, box, U(36), x), false);
        focusOn(F_SEARCH);
        return;
    }
    case H_LOGIN: submit(); return;
    case H_LINK:
        signup = !signup;
        err[0] = '\0';
        redraw();
        return;
    case H_NAV:
        if (view != idx || nowView) viewT = 0;
        view = idx;
        scrollY = scrollTarget = 0;
        nowView = false;
        break;
    case H_CARD:
        viewT = 0;
        view = PL_FIRST + idx;
        scrollY = scrollTarget = 0;
        break;
    case H_SIGNOUT: signOut(); return;
    case H_UNDO:
        setSkipped(toastId, false);
        toastLife = 0;
        break;
    case H_PLAYALL:
        if (viewCount > 0) { playSong(viewIdx[0]); playing = true; }
        break;
    case H_SHUFFLE:
    case H_BARSHUF: shuffleAll(); playing = true; break;
    case H_ROW: playSong(viewIdx[idx]); playing = true; if (view != 0) scrollY = scrollTarget = 0; break;
    case H_HEART: toggleLike(songList[viewIdx[idx]].id); break;
    case H_BARHEART: toggleLike(songList[current].id); break;
    case H_BARINFO: nowView = !nowView; break;
    case H_NPCLOSE: nowView = false; break;
    case H_REPEAT: repeatOne = !repeatOne; break;
    case H_PREV: previousSong(); break;
    case H_NEXT: nextSong(); break;
    case H_PLAY: playing = !playing; break;
    case H_SEEK: {
        R sk = rSeek(g);
        float ratio = (x - sk.x) / sk.w;
        if (ratio < 0) ratio = 0;
        if (ratio > 0.999f) ratio = 0.999f;
        setPos(ratio * getLength(songList[current].id));
        break;
    }
    default: return;
    }
    refresh();
}

void addMenuItem(int what, const char* label) {
    menuIds[menuCount] = what;
    snprintf(menuLabels[menuCount], 40, "%s", label);
    menuCount++;
}

void rightClick(float x, float y) {
    if (screen != SCREEN_APP) return;
    closeMenu();
    int target = hitTest(x, y);
    if (target != H_ROW && target != H_HEART) {
        redraw();
        return;
    }
    menuSong = viewIdx[hotIdx];
    menuCount = 0;
    int id = songList[menuSong].id;
    addMenuItem(M_PLAY, "Play");
    addMenuItem(M_NEXT, "Play Next");
    addMenuItem(M_LATER, "Play Later");
    if (menuSong > current) addMenuItem(M_QUEUE, isSkipped(id) ? "Add Back to Up Next" : "Remove from Up Next");
    menuSep = menuCount - 1;
    addMenuItem(M_LIKE, isLiked(id) ? "Remove from Favourites" : "Add to Favourites");
    addMenuItem(M_COPY, "Copy Song Name");
    menuX = x;
    menuY = y;
    menuOpen = true;
    menuT = 0;
    for (int i = 0; i < 8; i++) menuHv[i] = 0;
    hot = H_NONE;
    redraw();
}

void finishSwipe() {
    if (swipeRow < 0) return;
    int row = swipeRow;
    if (swipeX < -U(110)) {
        int index = viewIdx[row];
        removeFromQueue(index);
        if (view == 1) {
            shiftIdx = row;
            shiftAmt = geo().rh;
            swipeRow = -1;
            swipeX = 0;
            refresh();
            return;
        }
    }
    swipeHeld = false;
}

void onKey(WPARAM vk) {
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (screen == SCREEN_LOGIN) {
        if (vk == VK_RETURN) { submit(); return; }
        if (vk == VK_TAB) { focusOn(focusField == F_USER ? F_PASS : F_USER); return; }
    }
    Field* f = fieldOf(focusField);
    if (f) {
        caretOn = true;
        if (vk == VK_ESCAPE && focusField == F_SEARCH) { clearField(f); focusField = F_NONE; refresh(); return; }
        if (vk == VK_BACK) { if (hasSelection(f)) deleteSelection(f); else if (f->caret > 0) { f->anchor = f->caret - 1; deleteSelection(f); } }
        else if (vk == VK_DELETE) { if (hasSelection(f)) deleteSelection(f); else if (f->caret < f->len) { f->anchor = f->caret + 1; deleteSelection(f); } }
        else if (vk == VK_LEFT) moveCaret(f, hasSelection(f) && !shift ? (f->anchor < f->caret ? f->anchor : f->caret) : f->caret - 1, shift);
        else if (vk == VK_RIGHT) moveCaret(f, hasSelection(f) && !shift ? (f->anchor > f->caret ? f->anchor : f->caret) : f->caret + 1, shift);
        else if (vk == VK_HOME) moveCaret(f, 0, shift);
        else if (vk == VK_END) moveCaret(f, f->len, shift);
        else if (ctrl && vk == 'A') { f->anchor = 0; f->caret = f->len; }
        else if (ctrl && vk == 'V') pasteInto(f);
        else if (ctrl && vk == 'C' && focusField != F_PASS && hasSelection(f)) {
            char out[160];
            Field part = *f;
            int a = f->anchor < f->caret ? f->anchor : f->caret, b = f->anchor < f->caret ? f->caret : f->anchor;
            part.len = b - a;
            memmove(part.text, f->text + a, part.len * sizeof(wchar_t));
            part.text[part.len] = L'\0';
            fieldText(part, out, 160);
            copyText(out);
        } else return;
        if (focusField == F_SEARCH) { scrollY = scrollTarget = 0; refresh(); }
        redraw();
        return;
    }
    if (screen != SCREEN_APP) return;
    switch (vk) {
    case VK_ESCAPE:
        if (menuOpen) closeMenu();
        else nowView = false;
        break;
    case VK_SPACE: playing = !playing; break;
    case VK_RIGHT: nextSong(); break;
    case VK_LEFT: previousSong(); break;
    case 'S': shuffleAll(); break;
    case 'L': toggleLike(songList[current].id); break;
    case 'R': repeatOne = !repeatOne; break;
    case 'N': nowView = !nowView; break;
    case VK_OEM_2: focusOn(F_SEARCH); return;
    default: return;
    }
    refresh();
}

void onChar(wchar_t c) {
    Field* f = fieldOf(focusField);
    if (!f || c < 32 || c == 127) return;
    insertChar(f, c);
    caretOn = true;
    if (focusField == F_SEARCH) { scrollY = scrollTarget = 0; refresh(); }
    redraw();
}

// ---------- animation clock ----------

bool tick() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = lastQpc ? (float)(now.QuadPart - lastQpc) / (float)qpcFreq : 0.0f;
    lastQpc = now.QuadPart;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0) dt = 0;
    animT += dt;

    bool anim = false;
    for (int c = 0; c < H_COUNT; c++) {
        bool over = hot == c && c != H_NAV && c != H_ROW && c != H_HEART && c != H_MENU;
        anim |= approach(hv[c], over ? 1.0f : 0.0f, 16, dt);
        anim |= approach(pv[c], 0.0f, 14, dt);
    }
    anim |= approach(fieldFocus[0], focusField == F_USER ? 1.0f : 0.0f, 16, dt);
    anim |= approach(fieldFocus[1], focusField == F_PASS ? 1.0f : 0.0f, 16, dt);

    if (screen == SCREEN_LOGIN) {
        anim |= approach(loginT, 1.0f, 4.5f, dt);
        if (shakeT < 1) { shakeT = fminf(1.0f, shakeT + dt * 2.4f); anim = true; }
        onlyPlayback = false;
        return anim;
    }

    if (playing) {
        posf += dt;
        seconds = (int)posf;
        if (seconds >= getLength(songList[current].id)) {
            if (repeatOne) setPos(0);
            else { nextSong(); buildView(); clampScroll(); }
        }
    }
    if (songList[current].id != lastSongId) {
        lastSongId = songList[current].id;
        songT = 0;
    }
    anim |= approach(songT, 1.0f, 7, dt);
    anim |= approach(playIconT, playing ? 1.0f : 0.0f, 18, dt);
    anim |= approach(appFade, 1.0f, 3.5f, dt);
    anim |= approach(viewT, 1.0f, 3.6f, dt);
    anim |= approach(npT, nowView ? 1.0f : 0.0f, 5.5f, dt);

    for (int i = 0; i < NAV_COUNT; i++) {
        anim |= approach(navHv[i], hot == H_NAV && hotIdx == i && !(view == i && !nowView) ? 1.0f : 0.0f, 16, dt);
        anim |= approach(navSel[i], view == i && !nowView ? 1.0f : 0.0f, 14, dt);
    }
    for (int p = 0; p < PL_COUNT; p++)
        anim |= approach(cardHv[p], hot == H_CARD && hotIdx == p ? 1.0f : 0.0f, 16, dt);
    for (int i = 0; i < viewCount && i < 1000; i++)
        anim |= approach(rowHv[i], (hot == H_ROW || hot == H_HEART) && hotIdx == i ? 1.0f : 0.0f, 18, dt);
    for (int i = 0; i < menuCount; i++)
        anim |= approach(menuHv[i], hot == H_MENU && hotIdx == i ? 1.0f : 0.0f, 22, dt);
    if (menuOpen) anim |= approach(menuT, 1.0f, 9, dt);

    float before = scrollY;
    anim |= approach(scrollY, scrollTarget, 14, dt);
    if (fabsf(scrollY - before) > 0.01f && tracking) hot = hitTest(mouseX, mouseY);

    if (popT < 1) { popT = fminf(1.0f, popT + dt * 3.2f); anim = true; }
    if (shiftAmt > 0.5f) anim |= approach(shiftAmt, 0.0f, 13, dt);
    else if (shiftIdx >= 0) { shiftAmt = 0; shiftIdx = -1; }

    if (toastLife > 0) { toastLife -= dt; anim = true; }
    anim |= approach(toastAmt, toastLife > 0 ? 1.0f : 0.0f, 12, dt);

    if (swipeRow >= 0 && !swipeHeld) {
        if (lastHWheel != 0 && GetTickCount64() - lastHWheel > 220) { finishSwipe(); lastHWheel = 0; }
        if (swipeRow >= 0) {
            anim |= approach(swipeX, 0.0f, 16, dt);
            if (fabsf(swipeX) < 0.5f && lastHWheel == 0) { swipeRow = -1; swipeX = 0; }
        }
        anim = true;
    }

    float tr, tg, tb;
    ambientTarget(&tr, &tg, &tb);
    float k = motion ? 1.0f - expf(-dt * 4.0f) : 1.0f;
    amR += (tr - amR) * k;
    amG += (tg - amG) * k;
    amB += (tb - amB) * k;
    if (fabsf(tr - amR) + fabsf(tg - amG) + fabsf(tb - amB) > 0.5f) anim = true;

    onlyPlayback = playing && !anim;
    return anim || playing;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hMain = hwnd;
        signup = !hasUsers();
        shuffleAll();
        buildView();
        SetTimer(hwnd, 1, 530, NULL);
        focusField = F_USER;
        return 0;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && rt && LOWORD(lParam) > 0 && HIWORD(lParam) > 0)
            rt->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
        if (hMain) clampScroll();
        redraw();
        return 0;

    case WM_GETMINMAXINFO: {
        RECT r = {0, 0, (int)U(900), (int)U(620)};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = r.right - r.left;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }

    case WM_TIMER:
        caretOn = !caretOn;
        if (focusField != F_NONE) redraw();
        return 0;

    case WM_KEYDOWN:
        onKey(wParam);
        return 0;

    case WM_CHAR:
        onChar((wchar_t)wParam);
        return 0;

    case WM_LBUTTONDOWN: {
        float x = (float)(short)LOWORD(lParam), y = (float)(short)HIWORD(lParam);
        SetFocus(hwnd);
        mouseX = x; mouseY = y;
        int target = hitTest(x, y);
        if (target == H_ROW && !menuOpen && screen == SCREEN_APP) {
            pressRow = hotIdx;
            pressX0 = x; pressY0 = y;
            dragging = false;
            SetCapture(hwnd);
            return 0;
        }
        click(x, y);
        return 0;
    }

    case WM_LBUTTONUP: {
        float x = (float)(short)LOWORD(lParam), y = (float)(short)HIWORD(lParam);
        if (GetCapture() == hwnd) ReleaseCapture();
        if (pressRow >= 0) {
            int row = pressRow;
            pressRow = -1;
            if (dragging) {
                dragging = false;
                swipeHeld = false;
                finishSwipe();
                redraw();
                return 0;
            }
            if (hitTest(x, y) == H_ROW && hotIdx == row) click(x, y);
        }
        return 0;
    }

    case WM_RBUTTONDOWN:
        rightClick((float)(short)LOWORD(lParam), (float)(short)HIWORD(lParam));
        return 0;

    case WM_MOUSEMOVE: {
        float x = (float)(short)LOWORD(lParam), y = (float)(short)HIWORD(lParam);
        if (!tracking) {
            TRACKMOUSEEVENT t = {sizeof t, TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&t);
            tracking = true;
        }
        mouseX = x; mouseY = y;
        if (pressRow >= 0 && (wParam & MK_LBUTTON)) {
            float dx = x - pressX0, dy = y - pressY0;
            if (!dragging && fabsf(dx) > U(8) && fabsf(dx) > fabsf(dy) * 1.5f && swipeable(pressRow)) {
                dragging = true;
                swipeRow = pressRow;
                swipeHeld = true;
            }
            if (dragging) swipeX = dx < -U(240) ? -U(240) : (dx > 0 ? 0 : dx);
        }
        if (!dragging) hot = hitTest(x, y);
        redraw();
        return 0;
    }

    case WM_MOUSELEAVE:
        tracking = false;
        hot = H_NONE;
        redraw();
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            bool text = hot == H_SEARCH || hot == H_USERFIELD || hot == H_PASSFIELD;
            SetCursor(LoadCursor(NULL, hot != H_NONE ? (text ? IDC_IBEAM : IDC_HAND) : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEWHEEL:
        if (screen == SCREEN_APP) {
            closeMenu();
            scrollTarget -= (float)GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f * U(96);
            clampScroll();
            redraw();
        }
        return 0;

    case WM_MOUSEHWHEEL:
        if (screen == SCREEN_APP && !nowView) {
            POINT p = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
            ScreenToClient(hwnd, &p);
            int target = hitTest((float)p.x, (float)p.y);
            if (target == H_ROW && swipeable(hotIdx)) {
                swipeRow = hotIdx;
                swipeHeld = false;
                swipeX += (float)GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f * U(36);
                if (swipeX > 0) swipeX = 0;
                if (swipeX < -U(240)) swipeX = -U(240);
                lastHWheel = GetTickCount64();
                redraw();
            }
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        redraw();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        windowAlive = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

IDWriteTextFormat* makeFont(float px, DWRITE_FONT_WEIGHT weight) {
    IDWriteTextFormat* f = NULL;
    dw->CreateTextFormat(L"Segoe UI", NULL, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, px, L"en-us", &f);
    if (!f) return NULL;
    f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    DWRITE_TRIMMING trimming = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    IDWriteInlineObject* sign = NULL;
    dw->CreateEllipsisTrimmingSign(f, &sign);
    f->SetTrimming(&trimming, sign);
    if (sign) sign->Release();
    return f;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show) {
    SetProcessDPIAware();
    HDC screenDc = GetDC(NULL);
    dpi = GetDeviceCaps(screenDc, LOGPIXELSX);
    ReleaseDC(NULL, screenDc);
    ui = dpi / 96.0f;
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    qpcFreq = freq.QuadPart;

    char folder[MAX_PATH];
    GetModuleFileNameA(NULL, folder, MAX_PATH);
    char* slash = strrchr(folder, '\\');
    if (slash) {
        *slash = '\0';
        SetCurrentDirectoryA(folder);
    }
    srand((unsigned)time(NULL));

    Playlist playlist;
    if (loadPlaylist("playlist.txt", playlist) == 0) {
        MessageBoxA(NULL, "playlist.txt is missing or empty", "Smart Music Shuffler", MB_OK | MB_ICONERROR);
        return 1;
    }
    total = playlist.getSongCount();
    songList = new Song[total];
    playlist.toArray(songList, total);

    BOOL animations = TRUE;
    SystemParametersInfoA(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    motion = animations != FALSE;

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), NULL, (void**)&d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&dw);
    d2d->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f), NULL, 0, &roundStroke);
    tHuge = makeFont(U(44), DWRITE_FONT_WEIGHT_BOLD);
    tHero = makeFont(U(34), DWRITE_FONT_WEIGHT_BOLD);
    tLogin = makeFont(U(28), DWRITE_FONT_WEIGHT_BOLD);
    tSub = makeFont(U(20), DWRITE_FONT_WEIGHT_NORMAL);
    tBody = makeFont(U(15), DWRITE_FONT_WEIGHT_NORMAL);
    tBold = makeFont(U(15), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    tSmall = makeFont(U(13), DWRITE_FONT_WEIGHT_NORMAL);
    tInitS = makeFont(U(18), DWRITE_FONT_WEIGHT_BOLD);
    tInitL = makeFont(U(26), DWRITE_FONT_WEIGHT_BOLD);
    tInitXL = makeFont(U(96), DWRITE_FONT_WEIGHT_BOLD);
    tCover = makeFont(U(22), DWRITE_FONT_WEIGHT_BOLD);
    if (tCover) {
        tCover->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        tCover->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"ShufflerWindow";
    RegisterClassW(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT size = {0, 0, (int)U(1060), (int)U(680)};
    AdjustWindowRect(&size, style, FALSE);
    HWND hwnd = CreateWindowW(L"ShufflerWindow", L"Music Shuffler", style, CW_USEDEFAULT, CW_USEDEFAULT,
                              size.right - size.left, size.bottom - size.top, NULL, NULL, instance, NULL);
    BOOL dark = TRUE;
    COLORREF caption = RGB(18, 18, 20);
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
    DwmSetWindowAttribute(hwnd, 35, &caption, sizeof caption);
    ShowWindow(hwnd, show);

    MSG msg;
    while (windowAlive) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) windowAlive = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!windowAlive) break;
        if (IsIconic(hwnd)) {
            WaitMessage();
            continue;
        }
        bool busy = tick();
        if (busy || needDraw) {
            needDraw = false;
            LARGE_INTEGER a, b;
            QueryPerformanceCounter(&a);
            render();
            QueryPerformanceCounter(&b);
            float ms = (float)(b.QuadPart - a.QuadPart) * 1000.0f / (float)qpcFreq;
            if (onlyPlayback && GetForegroundWindow() != hwnd) Sleep(30);
            else if (ms < 4.0f && busy) Sleep(1);
        } else {
            WaitMessage();
        }
    }

    destroyTarget();
    IDWriteTextFormat* fonts[11] = {tHuge, tHero, tLogin, tSub, tBody, tBold, tSmall, tInitS, tInitL, tInitXL, tCover};
    for (int i = 0; i < 11; i++) if (fonts[i]) fonts[i]->Release();
    if (roundStroke) roundStroke->Release();
    if (dw) dw->Release();
    if (d2d) d2d->Release();
    delete[] songList;
    return 0;
}
