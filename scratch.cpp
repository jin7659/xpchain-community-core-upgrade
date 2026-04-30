#include <iostream>
int main() {
    unsigned int nCompact = 0x207fffff;
    int nSize = nCompact >> 24;
    unsigned int nWord = nCompact & 0x007fffff;
    std::cout << "Size: " << nSize << " Word: " << std::hex << nWord << std::endl;
    return 0;
}
