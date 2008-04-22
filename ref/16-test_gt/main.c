#include <stdio.h>
int _start(){
  volatile int a,b,c;
  a = 13;
  b = 7;
  if(a-b > 2){
    c = a + b;
  }else{
    c = a - b;
  }
  return c;
}

