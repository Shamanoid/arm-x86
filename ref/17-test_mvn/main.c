#include <stdio.h>
int _start(){
  volatile int a,b,c;
  a = 13;
  b = 7;
  if((a == 13) && (b == 7)){
    c = ~b;
  }else{
    c = a + ~b;
  }
  return c;
}

