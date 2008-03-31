#include <stdio.h>
int _start(){
  volatile int a,b,c;
  a = 0;
  b = 3;
  if(a > b){
    c = a + b;
  }else{
    c = b - a;
  }
  return c;
}

