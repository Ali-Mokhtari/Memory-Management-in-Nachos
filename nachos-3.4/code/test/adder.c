#include "syscall.h"
#define loops 2000

int main() {
    int i,x;
    x = 0;
    for (i=0; i<loops; i++){
        x = x +i;
        
    }
    Write("\n Adding Complete\n",20,ConsoleOutput);
    Exit(0);
}