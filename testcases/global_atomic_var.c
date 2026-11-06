// RUN: %bchk %s -- -I headers

#include "borrowck.h"

_Atomic int a = 0;
int main(int argc, char OWNER **argv) {
    return a;
}