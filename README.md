# read_write_binary_cpp_dummies
A simple, intuitive and very little library to read and write binary files.

It allows to manage binary files in a high-level way.

# Usage

```C++
#include <iostream>
#include <vector>
#include <algorithm>
#include "readwritebin.h"

int main() {
  // Create the file
  Bin binfile("binfile.bin");

  // Write 4 integers (since the values
  // are already integers you can omit the
  // "<int>" specification)
  binfile.write_many<int>({0, 2, 5, 9});

  // Place the cursor 3 integers behind
  binfile.rmove_by<int>(-3);

  // Read the following 3 integers
  std::vector<int> values = binfile.get_values<int>(3);

  // Print them on the screen
  std::cout << "The first time the numbers are: ";
  for (auto a : values) std::cout << a << " ";
  std::cout << std::endl;

  // Now let's reverse this three numbers in the file
  // The number 2 is the second integer number, so
  // its position, in terms of integers, is 1
  std::reverse(binfile.begin<int>() + 1, binfile.end<int>());

  // Read all 4 numbers from the beginning (position 0)
  values = binfile.get_values<int>(4, 0);

  // Print them on the screen
  std::cout << "The second time the numbers are: ";
  for (auto a : values) std::cout << a << " ";
  std::cout << std::endl;

  return 0;
}

```

Output:
```
The first time the numbers are: 2 5 9 
The second time the numbers are: 0 9 5 2 
```

# Documentation
If not installed, install Doxygen:
```
sudo apt-get install doxygen
```
Clone the repo:
```
git clone https://github.com/PaoloLRinaldi/read_write_binary_cpp_dummies.git
```
Go inside the directory:
```
cd read_write_binary_cpp_dummies
```
Generate the documentation:
```
doxygen doxyconfig.txt
```
To open the documentation, if you have firefox write:
```
firefox html/index.html
```
If you have Chrome wirte:
```
google-chrome html/index.html
```

# Requirements
C++11
