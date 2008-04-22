#include <stdio.h>
int _start(){
  volatile int a,b,c;
  a = 13;
  b = 7;
  for(a = 0; a < b && a < b-a; a++){
    if((a == 7) && (b == 13)){
      c = a & ~b;
    }else{
      c = a & b;
    }
  }
  return c;
}

