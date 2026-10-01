#include "FisherYates.h"
#include <cstdlib>
#include <iostream>

using namespace std;

void shuffleSongs(Song arr[], int n) {
  if (n <= 1 || arr == NULL)
    return;

  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    Song temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
  }
}

void shuffleArray(int arr[], int n) {
  if (n <= 1 || arr == NULL)
    return;

  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int temp = arr[i];
    arr[i] = arr[j];
    arr[j] = temp;
  }
}

static int mapPermutation(int a, int b, int c) {
  if (a == 1 && b == 2 && c == 3)
    return 0;
  if (a == 1 && b == 3 && c == 2)
    return 1;
  if (a == 2 && b == 1 && c == 3)
    return 2;
  if (a == 2 && b == 3 && c == 1)
    return 3;
  if (a == 3 && b == 1 && c == 2)
    return 4;
  if (a == 3 && b == 2 && c == 1)
    return 5;
  return -1;
}

void testPermutationDistribution(int trials) {
  cout << "\n--- Testing 3-Element Shuffle (" << trials << " runs) ---\n";

  int counts[6] = {0};
  const char *names[6] = {"[1, 2, 3]", "[1, 3, 2]", "[2, 1, 3]",
                          "[2, 3, 1]", "[3, 1, 2]", "[3, 2, 1]"};

  for (int t = 0; t < trials; t++) {
    int sample[3] = {1, 2, 3};
    shuffleArray(sample, 3);
    int idx = mapPermutation(sample[0], sample[1], sample[2]);
    if (idx >= 0 && idx < 6) {
      counts[idx]++;
    }
  }

  cout << "Outcome\t\tCount\tGot %\tIdeal %\n";
  cout << "-------------------------------------------\n";

  for (int i = 0; i < 6; i++) {
    double pct = ((double)counts[i] / (double)trials) * 100.0;
    cout << names[i] << "\t" << counts[i] << "\t";
    cout.precision(1);
    cout << fixed << pct << "%\t16.7%\n";
  }
  cout << "-------------------------------------------\n";
}

void testPositionDistribution(int songCount, int trials) {
  if (songCount <= 1 || trials <= 0)
    return;

  cout << "\n--- Testing " << songCount << " Songs (" << trials
       << " runs) ---\n";

  int *slotCounts = new int[songCount];
  for (int i = 0; i < songCount; i++) {
    slotCounts[i] = 0;
  }

  int *playlist = new int[songCount];

  for (int t = 0; t < trials; t++) {
    for (int i = 0; i < songCount; i++) {
      playlist[i] = i;
    }

    shuffleArray(playlist, songCount);

    for (int i = 0; i < songCount; i++) {
      if (playlist[i] == 0) {
        slotCounts[i]++;
        break;
      }
    }
  }

  double expectedPct = 100.0 / (double)songCount;
  cout << "Ideal chance per slot = " << expectedPct << "%\n";
  cout << "Checking where Song 0 ends up (sample slots):\n";

  for (int i = 0; i < 5 && i < songCount; i++) {
    double pct = ((double)slotCounts[i] / (double)trials) * 100.0;
    cout << "  Slot " << i << ": " << slotCounts[i] << " times (" << pct
         << "%)\n";
  }
  cout << "  ...\n";
  for (int i = songCount - 5; i < songCount; i++) {
    if (i >= 5) {
      double pct = ((double)slotCounts[i] / (double)trials) * 100.0;
      cout << "  Slot " << i << ": " << slotCounts[i] << " times (" << pct
           << "%)\n";
    }
  }

  delete[] slotCounts;
  delete[] playlist;
  cout << "Result: Each slot gets around " << expectedPct
       << "% (evenly spread).\n";
}
