#include "kernel.h"

int main(void)
{
    CKernel Kernel;

    if (!Kernel.Initialize())
        while(1) {} // halt if initialization fails

    Kernel.Run();

    return 0; // never reached
}
