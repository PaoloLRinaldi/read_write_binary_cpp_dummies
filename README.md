# read_write_binary_cpp_dummies
A simple, intuitive and very little library to read and write binary files.

It allows to manage binary files in a higl-level way.

```C++
#include <iostream>
#include <vector>
#include "readwritebin.h"

int main() {
Bin binfile("binfile.bin", false);

binfile.write_many<int>({0, 2, 5, 9});
binfile.rmove_by<int>(-3);

std::vector<int> values = binfile.get_values<int>(3);

for (a : values) std::cout << a << " ";
std::cout << std::endl;

return 0;
}

```

Output:
```
2 5 9 
```
