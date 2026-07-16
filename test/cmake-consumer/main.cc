#include <protocache/utils.h>

int main() {
    protocache::Buffer buffer;
    return buffer.Size() == 0 ? 0 : 1;
}
