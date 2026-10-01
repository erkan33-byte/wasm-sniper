#include "sniper.c"
#include <stdio.h>
#include <string.h>
#include <time.h>
static void h2b(const char*h,uint8_t*o,int n){for(int i=0;i<n;i++){unsigned v;sscanf(h+2*i,"%2x",&v);o[i]=v;}}
int main(){char s[80],t[48];int exp,ok=0,n=0;
 while(scanf("%79s %47s %d",s,t,&exp)==3){uint8_t st[32],tg[20];h2b(s,st,32);h2b(t,tg,20);init_sniper(st);int got=-1;
  for(int b=0;b<10;b++){uint32_t off;if(scan_batch(tg,NB,&off)==0){got=b*NB+(int)off;break;}}
  n++;printf("verwacht %d, gevonden %d %s\n",exp,got,got==exp?"OK":"FOUT");ok+=got==exp;}
 uint8_t st[32]={0},tg[20]={0};st[31]=1;init_sniper(st);clock_t c0=clock();long keys=0;uint32_t off;
 while((double)(clock()-c0)/CLOCKS_PER_SEC<2.0){scan_batch(tg,NB,&off);keys+=NB;}
 printf("%d/%d tests goed; %.0f sleutels/s (native, 1 thread)\n",ok,n,keys/((double)(clock()-c0)/CLOCKS_PER_SEC));return ok!=n;}
