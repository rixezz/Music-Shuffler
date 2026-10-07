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

void testPermutationDistribution(int trials) {
    cout << "\nTesting 3 elements over " << trials << " runs:" << endl;

    int counts[6] = {0};
    const char *names[6] = {"[1, 2, 3]", "[1, 3, 2]", "[2, 1, 3]",
                            "[2, 3, 1]", "[3, 1, 2]", "[3, 2, 1]"};

    for (int t = 0; t < trials; t++) {
        int s[3] = {1, 2, 3};
        shuffleArray(s, 3);

        if (s[0] == 1 && s[1] == 2)
            counts[0]++;
        else if (s[0] ==1 && s[1] == 3)
            counts[1]++;
        else if (s[0] == 2 && s[1] == 1)
            counts[2]++;
        else if (s[0] == 2 && s[1] ==3)
            counts[3]++;
        else if (s[0] == 3 && s[1] == 1)
            counts[4]++;
        else if (s[0] == 3 && s[1] == 2)
            counts[5]++;
    }

    cout << "Permutation\tCount\tGot %\tExpected" << endl;
    cout << "-------------------------------------------" << endl;

    for (int counter = 0; counter < 6; counter++) {
        double pc = (counts[counter] * 100.0) / trials;
        cout << names[counter] << "\t" << counts[counter] << "\t";
        cout.precision(1);
        cout << fixed << pc << "%\t16.7%" << endl;
    }
    cout << "-------------------------------------------" << endl;
}

void testPositionDistribution(int songCount, int trials) {
    if (songCount <= 1 || trials <=0)
        return;

    cout << "\nTesting " << songCount << " songs over " << trials << " runs:" << endl;

    int *slot = new int[songCount];
    for (int counter = 0; counter < songCount; counter++) {
        slot[counter] = 0;
    }

    int *playlist = new int[songCount];

    for (int t = 0; t < trials; t++) {
        for (int counter = 0; counter < songCount; counter++) {
            playlist[counter] = counter;
        }

        shuffleArray(playlist, songCount);

        for (int counter = 0; counter < songCount; counter++) {
            if (playlist[counter] ==0) {
                slot[counter]++;
                break;
            }
        }
    }

    double ep = 100.0 / songCount;
    cout << "Expected per slot: " << ep << "%" << endl;
    cout << "Sample slots for Song 0:" << endl;

    for (int counter = 0; counter < 5 && counter < songCount; counter++) {
        double pc = (slot[counter] * 100.0) / trials;
        cout << "Slot " << counter << ": " << slot[counter] << " times (" << pc
             << "%)" << endl;
    }
    cout << "..." << endl;
    for (int counter = songCount - 5; counter < songCount; counter++) {
        if (counter >= 5) {
            double pc = (slot[counter] * 100.0) / trials;
            cout << "Slot " << counter << ": " << slot[counter] << " times (" << pc
                 << "%)" << endl;
        }
    }

    delete[] slot;
    delete[] playlist;
    cout << "All slots spread evenly around " << ep << "%" << endl;
}

