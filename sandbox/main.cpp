#include "Toybox/Common.h"
#include "Toybox/LockFreeRingQueue.h"

TOYBOX_NAMESPACE_BEGIN

void sandbox() {
    LockFreeRingQueue<int, 8> queue;
    // Add sandbox code
}

TOYBOX_NAMESPACE_END

int main() {
    toybox::sandbox();
    return 0;
}
