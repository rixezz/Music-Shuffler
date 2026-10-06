#include "FisherYates.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>

using namespace std;

void shuffleSongs(Song items[], int length) {
    if (items == NULL || length <= 1)
        return;

    for (int counter =length - 1; counter > 0; counter--) {
        int inner = rand() % (counter + 1);
        Song temp =items[counter];
        items[counter] = items[inner];
        items[inner] = temp;
    }
}

void shuffleArray(int items[], int length) {
    if (items ==NULL || length <= 1)
        return;

    for (int counter = length -1; counter > 0; counter--) {
        int inner = rand() % (counter + 1);
        int temp = items[counter];
        items[counter] = items[inner];
        items[inner] = temp;
    }
}
