SMART PLAYLIST SHUFFLER

1\. PROJECT OVERVIEW

Smart Playlist Shuffler is a C++ console-based application that manages a music playlist using different data structures and algorithms. It allows users to view songs, search for tracks, shuffle the playlist, navigate between songs, and maintain a history of previously played songs.

The project demonstrates the practical implementation of circular doubly linked lists, stacks, hash sets, file handling, and the Fisher–Yates shuffle algorithm.

Note: The current player simulates playback and track progress. It does not play actual audio.

2\. FEATURES

1. Interactive Console Interface: Provides a menu-driven interface for accessing different features.
2. Playlist Management: Loads songs from a text file and stores them in a playlist.
3. View All Songs: Displays all songs available in the playlist.
4. Search Songs: Searches for songs by title or artist using case-insensitive matching.
5. Shuffle Playlist: Uses the Fisher–Yates algorithm to shuffle songs.
6. Next and Previous Navigation: Allows users to move between tracks and revisit previously played songs.
7. Pause and Resume: Simulates pausing and resuming the current track.
8. Play History: Maintains a history of recently played songs using a stack.
9. Up Next: Displays songs scheduled to play next.
10. Played Song Tracking: Uses a custom hash set to track song IDs played during the current shuffle cycle.

3\. DATA STRUCTURES AND ALGORITHMS USED

A. Circular Doubly Linked List

The playlist is implemented using a circular doubly linked list.

Each node contains song information, a pointer to the next node, and a pointer to the previous node.

Operations include:

- Adding songs
- Removing songs
- Displaying all songs
- Counting songs
- Converting the playlist into an array
- Rebuilding the playlist from an array

Advantages:

- Supports traversal in both directions.
- Allows efficient insertion and deletion when the relevant node is available.
- Maintains connections between the first and last nodes.

B. Fisher–Yates Shuffle Algorithm

The Fisher–Yates algorithm randomly rearranges the songs in the playlist.

Working:

1. Start from the last element of the array.
2. Select a random index between zero and the current index.
3. Swap the current element with the selected element.
4. Repeat until the beginning of the array is reached.

Complexity:

- Time Complexity: O(n)
- Auxiliary Space Complexity: O(1)

The implementation uses rand() % (i + 1) to select random indices, which can introduce statistical bias.

C. Custom Hash Set

A custom hash set is used to track song IDs that have already been played during the current shuffle cycle.

Features:

- Insert song IDs
- Check whether a song has been played
- Clear the set
- Retrieve the number of stored elements
- Display stored elements
- Resize and rehash when the load factor exceeds 70%

The hash set uses linear probing to handle collisions.

D. Stack for Play History

The HistoryStack class stores recently played songs using an array-based stack.

Operations include:

- push(): Adds a song to the history.
- pop(): Removes the top song.
- peek(): Returns the top song without removing it.
- peekAt(): Accesses a song at a specified depth.
- isEmpty(): Checks whether the stack is empty.
- getSize(): Returns the number of stored songs.
- display(): Displays the play history.

The stack follows the LIFO (Last In, First Out) principle.

E. File Handling

The FileParser module loads song information from playlist.txt.

Each line follows this format:

id|title|artist

Example:

0|Blinding Lights|The Weeknd

The parser reads the file and adds valid song records to the playlist.

4\. PROJECT STRUCTURE

main.cpp Contains the main program, menu system, player controls, search functionality, and navigation.

Playlist.h Declares the playlist class and its node structure.

Playlist.cpp Implements playlist operations using a circular doubly linked list.

FisherYates.h Declares the song structure and shuffle-related functions.

FisherYates.cpp Implements the Fisher–Yates shuffle algorithm and shuffle distribution tests.

CustomHashSet.h Declares the custom hash set class.

CustomHashSet.cpp Implements hashing, collision handling, insertion, searching, and rehashing.

HistoryStack.h Declares the history stack class.

HistoryStack.cpp Implements stack operations for recently played songs.

FileParser.h Declares the playlist file-loading function.

FileParser.cpp Implements file parsing and playlist loading.

playlist.txt Contains the song records.

README.md Contains project documentation.

5\. REQUIREMENTS

- Windows operating system
- C++ compiler, such as MinGW-w64
- Command Prompt or terminal
- playlist.txt available in the program's working directory

The application uses Windows-specific headers and functions, including windows.h and conio.h.

6\. COMPILATION AND EXECUTION

Compile the project using the following command:

g++ main.cpp Playlist.cpp FisherYates.cpp CustomHashSet.cpp HistoryStack.cpp FileParser.cpp -o shuffler.exe

Run the program using:

shuffler.exe

Make sure playlist.txt is present in the current working directory before running the application.

7\. MAIN MENU

The application provides the following menu options:

1. Live Player
2. All Songs
3. Search
4. Exit

Users can select an option to access the corresponding functionality.

8\. KEYBOARD CONTROLS

N - Play the next song

P - Play the previous song

S - Shuffle the playlist again

Space - Pause or resume playback simulation

H - Display play history

U - Display the Up Next list

M - Return to the main menu

Backspace - Return from supported list screens

9\. SHUFFLE VALIDATION

The project includes utility functions for evaluating the shuffle algorithm.

testPermutationDistribution(int trials): Examines the distribution of permutations for three elements.

testPositionDistribution(int songCount, int trials): Tracks the position of a selected integer across repeated shuffles.

These tests provide empirical observations about shuffle distribution. They do not mathematically prove that the implementation is perfectly random. The testing functions are not currently exposed through the main menu.

10\. LEARNING OUTCOMES

Through this project, we demonstrate:

- Implementation of circular doubly linked lists
- Stack-based history management
- Hash sets and collision resolution
- Application of the Fisher–Yates shuffle algorithm
- File handling and parsing in C++
- Dynamic memory allocation and management
- Modular programming using header and source files
- Menu-driven console application development
- Basic testing of randomized algorithms

11\. FUTURE IMPROVEMENTS

- Integrating an actual audio playback library
- Adding volume control and playback progress
- Allowing users to create and save multiple playlists
- Improving random number generation to reduce shuffle bias
- Adding song filtering by genre, artist, or duration
- Saving play history between sessions
- Improving cross-platform compatibility
- Adding automated unit tests for all data structures

12\. CONCLUSION

Smart Playlist Shuffler demonstrates how fundamental data structures and algorithms can be combined to develop a practical playlist management application. By integrating a circular doubly linked list, a custom hash set, a stack, file handling, and the Fisher–Yates shuffle algorithm, the project provides a hands-on example of applying data structures and algorithmic concepts in C++.

The project also creates a foundation for future improvements, including real audio playback and more advanced playlist management features.
