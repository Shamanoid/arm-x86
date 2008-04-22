#include <stdio.h>
int main(){
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
  printf("%x\n",c);
  return c;
}

