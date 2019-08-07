// toolkit.h

#include "toolkit.h"


unsigned int abs(int x) {
    if (x < 0) return -x;
    return x;
}    


int floor_sqrt(int x) { 
    // Base cases 
    if (x == 0 || x == 1) 
    return x; 
    // Staring from 1, try all numbers until 
    // i*i is greater than or equal to x. 
    int i = 1, result = 1;
    while (result <= x) { 
      i++; 
      result = i * i; 
    } 
    return i - 1; 
}
