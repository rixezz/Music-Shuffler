#ifndef FISHER_YATES_H
#define FISHER_YATES_H

struct Song {
    int id;
    char title[120];
    char artist[80];
};

void shuffleSongs(Song arr[], int n);
void shuffleArray(int arr[], int n);
void testPermutationDistribution(int trials);
void testPositionDistribution(int songCount, int trials);

#endif
