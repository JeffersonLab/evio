#include <cstdlib>
#include <cstdio>
#include <string>

#include <iostream>
#include <filesystem>

int main() {
#ifdef __cpp_lib_filesystem
    std::cout << "Filesystem is supported." << std::endl;
#else
    std::cout << "Filesystem is not supported." << std::endl;
#endif
    return 0;
}
