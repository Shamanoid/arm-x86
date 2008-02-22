#include <stdio.h>
int foo();
int foo(){
  volatile int a,b,c;
  a = 0;
  b = 3;
  c = a + b;
  return c;
}
int main(){
  return foo();
}

