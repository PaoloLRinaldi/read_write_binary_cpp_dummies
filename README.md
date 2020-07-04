# read_write_binary_cpp_dummies
A simple, intuitive and very little library to read and write binary files.

It allows to manage binary files in a higl-level way.

# Usage

```C++
#include <iostream>
#include <vector>
#include <algorithm>
#include "readwritebin.h"

int main() {
// Create the file
Bin binfile("binfile.bin", false);

// Write 4 integers
binfile.write_many<int>({0, 2, 5, 9});

// Place the cursor 3 integers behind
binfile.rmove_by<int>(-3);

// Read the following 3 integers
std::vector<int> values = binfile.get_values<int>(3);

// Print them on the screen
std::cout << "The first time the numbers are: ";
for (a : values) std::cout << a << " ";
std::cout << std::endl;

// Now let's reverse this three numbers in the file
// The number two is the second integer number, so
// its position, in terms of integers, is 1
std::reverse(binfile.begin<int>() + 1, binfile.end<int>());

// Read all 4 numbers from the beginning (position 0)
values = binfile.get_values<int>(3, 0);

// Print them on the screen
std::cout << "The second time the numbers are: ";
for (a : values) std::cout << a << " ";
std::cout << std::endl;

return 0;
}

```

Output:
```
The first time the numbers are: 2 5 9 
The first time the numbers are: 0 9 5 2 
```
