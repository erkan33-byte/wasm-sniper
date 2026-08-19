#include <stdint.h>
#include <stddef.h>
void *memset(void *s,int c,size_t n){unsigned char *p=s;while(n--)*p++=(unsigned char)c;return s;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}

#define MAX_BATCH 10000

typedef unsigned __int128 u128;
typedef struct { uint64_t v[4]; } fe;
typedef struct { fe x,y,z; int inf; } jac;

static const fe FP = {{0xFFFFFFFEFFFFFC2FULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL}};
static const fe GX = {{0x59F2815B16F81798ULL,0x029BFCDB2DCE28D9ULL,0x55A06295CE870B07ULL,0x79BE667EF9DCBBACULL}};
static const fe GY = {{0x9C47D08FFB10D4B8ULL,0xFD17B448A6855419ULL,0x5DA4FBFC0E1108A8ULL,0x483ADA7726A3C465ULL}};

static int cmp4(const uint64_t *a,const uint64_t *b){for(int i=3;i>=0;i--){if(a[i]>b[i])return 1;if(a[i]<b[i])return -1;}return 0;}
static void sub4(uint64_t *a,const uint64_t *b){uint64_t borrow=0;for(int i=0;i<4;i++){uint64_t x=a[i],y=b[i]+borrow;uint64_t c=(borrow?y<=b[i]:0);a[i]=x-y;borrow=(x<y)||c;}}
static void addwide(uint64_t *t,int p,uint64_t a){while(a&&p<10){u128 s=(u128)t[p]+a;t[p]=(uint64_t)s;a=(uint64_t)(s>>64);p++;}}
static void addwide128(uint64_t *t,int p,u128 a){addwide(t,p,(uint64_t)a);addwide(t,p+1,(uint64_t)(a>>64));}

/* p = 2^256 - 2^32 - 977. Fold every high limb by 2^256 = 2^32 + 977. */
static fe reduce10(uint64_t t[10]){
  for(int pass=0;pass<12;pass++) for(int i=9;i>=4;i--){uint64_t q=t[i];if(!q)continue;t[i]=0;addwide128(t,i-4,(u128)q*977);addwide128(t,i-4,(u128)q<<32);}
  fe r={{t[0],t[1],t[2],t[3]}};
  while(cmp4(r.v,FP.v)>=0) sub4(r.v,FP.v);
  return r;
}
static fe fadd(fe a,fe b){uint64_t t[10]={0};for(int i=0;i<4;i++){u128 s=(u128)a.v[i]+b.v[i]+t[i];t[i]=(uint64_t)s;addwide(t,i+1,(uint64_t)(s>>64));}return reduce10(t);}
static fe fsub(fe a,fe b){if(cmp4(a.v,b.v)>=0){fe r=a;sub4(r.v,b.v);return r;}fe n=FP;sub4(n.v,b.v);return fadd(a,n);}
static fe fmul(fe a,fe b){uint64_t t[10]={0};for(int i=0;i<4;i++){u128 carry=0;for(int j=0;j<4;j++){u128 s=(u128)a.v[i]*b.v[j]+t[i+j]+carry;t[i+j]=(uint64_t)s;carry=s>>64;}addwide128(t,i+4,carry);}return reduce10(t);}
static fe fsqr(fe a){return fmul(a,a);} 
static fe fscale(fe a,uint64_t n){fe r={{0,0,0,0}};for(uint64_t i=0;i<n;i++)r=fadd(r,a);return r;}
static int fzero(fe a){return !(a.v[0]|a.v[1]|a.v[2]|a.v[3]);}
static fe fpow(fe a){/* p-2 */static const uint64_t e[4]={0xFFFFFFFEFFFFFC2DULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL};fe r={{1,0,0,0}};for(int i=255;i>=0;i--){r=fsqr(r);if((e[i>>6]>>(i&63))&1ULL)r=fmul(r,a);}return r;}

static jac jbase(void){jac p={GX,GY,{{1,0,0,0}},0};return p;}
static jac jdouble(jac p){if(p.inf||fzero(p.y)) {p.inf=1;return p;}fe A=fsqr(p.x),B=fsqr(p.y),C=fsqr(B);fe D=fscale(fsub(fsqr(fadd(p.x,B)),fadd(A,C)),2);fe E=fscale(A,3),F=fsqr(E);jac r;r.x=fsub(F,fscale(D,2));r.y=fsub(fmul(E,fsub(D,r.x)),fscale(C,8));r.z=fscale(fmul(p.y,p.z),2);r.inf=0;return r;}
static jac jaddg(jac p){if(p.inf)return jbase();fe z2=fsqr(p.z),u2=fmul(GX,z2),s2=fmul(GY,fmul(p.z,z2));fe h=fsub(u2,p.x),rr=fsub(s2,p.y);if(fzero(h)){p.inf=1;return p;}fe hh=fsqr(h),hhh=fmul(h,hh),v=fmul(p.x,hh);jac q;q.x=fsub(fsub(fsqr(rr),hhh),fscale(v,2));q.y=fsub(fmul(rr,fsub(v,q.x)),fmul(p.y,hhh));q.z=fmul(p.z,h);q.inf=0;return q;}
static jac jmul(const uint8_t k[32]){jac r={{{0,0,0,0}},{{0,0,0,0}},{{0,0,0,0}},1};for(int i=0;i<256;i++){if(!r.inf)r=jdouble(r);if((k[i>>3]>>(7-(i&7)))&1)r=jaddg(r);}return r;}

static jac points[MAX_BATCH];
static fe prefix[MAX_BATCH],zinv[MAX_BATCH];
static jac current;

static void be32(uint8_t out[32],fe x){for(int i=0;i<4;i++)for(int j=0;j<8;j++)out[31-(i*8+j)]=(uint8_t)(x.v[i]>>(j*8));}

static uint32_t ror32(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
static const uint32_t K256[64]={
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static void sha33(const uint8_t in[33],uint8_t out[32]){
 uint32_t w[64];for(int i=0;i<8;i++)w[i]=((uint32_t)in[i*4]<<24)|((uint32_t)in[i*4+1]<<16)|((uint32_t)in[i*4+2]<<8)|in[i*4+3];w[8]=((uint32_t)in[32]<<24)|0x00800000;for(int i=9;i<15;i++)w[i]=0;w[15]=264;
 for(int i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(ror32(a,7)^ror32(a,18)^(a>>3))+w[i-7]+(ror32(b,17)^ror32(b,19)^(b>>10));}
 uint32_t a=0x6a09e667,b=0xbb67ae85,c=0x3c6ef372,d=0xa54ff53a,e=0x510e527f,f=0x9b05688c,g=0x1f83d9ab,h=0x5be0cd19;
 for(int i=0;i<64;i++){uint32_t t1=h+(ror32(e,6)^ror32(e,11)^ror32(e,25))+((e&f)^((~e)&g))+K256[i]+w[i],t2=(ror32(a,2)^ror32(a,13)^ror32(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
 uint32_t q[8]={a+0x6a09e667,b+0xbb67ae85,c+0x3c6ef372,d+0xa54ff53a,e+0x510e527f,f+0x9b05688c,g+0x1f83d9ab,h+0x5be0cd19};for(int i=0;i<8;i++){out[i*4]=q[i]>>24;out[i*4+1]=q[i]>>16;out[i*4+2]=q[i]>>8;out[i*4+3]=q[i];}
}


static uint32_t rol32(uint32_t x,int n){return (x<<n)|(x>>(32-n));}
static const uint8_t RL[80]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13};
static const uint8_t RR[80]={5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11};
static const uint8_t SL[80]={11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6};
static const uint8_t SR[80]={8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11};
static uint32_t fl(int r,uint32_t x,uint32_t y,uint32_t z){if(r==0)return x^y^z;if(r==1)return (x&y)|(~x&z);if(r==2)return (x|~y)^z;if(r==3)return (x&z)|(y&~z);return x^(y|~z);}
static uint32_t fr(int r,uint32_t x,uint32_t y,uint32_t z){if(r==0)return x^(y|~z);if(r==1)return (x&z)|(y&~z);if(r==2)return (x|~y)^z;if(r==3)return (x&y)|(~x&z);return x^y^z;}
static void ripemd32(const uint8_t in[32],uint8_t out[20]){
 uint32_t w[16];for(int i=0;i<8;i++)w[i]=(uint32_t)in[i*4]|((uint32_t)in[i*4+1]<<8)|((uint32_t)in[i*4+2]<<16)|((uint32_t)in[i*4+3]<<24);w[8]=0x80;for(int i=9;i<14;i++)w[i]=0;w[14]=256;w[15]=0;
 uint32_t al=0x67452301,bl=0xefcdab89,cl=0x98badcfe,dl=0x10325476,el=0xc3d2e1f0,ar=al,br=bl,cr=cl,dr=dl,er=el;
 for(int j=0;j<80;j++){int r=j>>4;uint32_t kl=r==0?0:r==1?0x5a827999:r==2?0x6ed9eba1:r==3?0x8f1bbcdc:0xa953fd4e;uint32_t kr=r==0?0x50a28be6:r==1?0x5c4dd124:r==2?0x6d703ef3:r==3?0x7a6d76e9:0;uint32_t t=rol32(al+fl(r,bl,cl,dl)+w[RL[j]]+kl,SL[j])+el;al=el;el=dl;dl=rol32(cl,10);cl=bl;bl=t;t=rol32(ar+fr(r,br,cr,dr)+w[RR[j]]+kr,SR[j])+er;ar=er;er=dr;dr=rol32(cr,10);cr=br;br=t;}
 uint32_t h[5]={0xefcdab89U + cl + dr,0x98badcfeU + dl + er,0x10325476U + el + ar,0xc3d2e1f0U + al + br,0x67452301U + bl + cr};for(int i=0;i<5;i++){out[i*4]=h[i];out[i*4+1]=h[i]>>8;out[i*4+2]=h[i]>>16;out[i*4+3]=h[i]>>24;}
}


static int equal20(const uint8_t a[20],const uint8_t b[20]){uint8_t d=0;for(int i=0;i<20;i++)d|=a[i]^b[i];return d==0;}
void init_sniper(const uint8_t *start){current=jmul(start);}
int scan_batch(const uint8_t *target,int count,uint32_t *found){
 if(count<1)return -1;if(count>MAX_BATCH)count=MAX_BATCH;
 jac next=current;for(int i=0;i<count;i++){points[i]=next;next=jaddg(next);}
 fe acc={{1,0,0,0}};
 for(int i=0;i<count;i++){prefix[i]=acc;acc=fmul(acc,points[i].z);}
 acc=fpow(acc);
 for(int i=count-1;i>=0;i--){zinv[i]=fmul(acc,prefix[i]);acc=fmul(acc,points[i].z);}
 uint8_t pub[33],sh[32],rh[20];
 for(int i=0;i<count;i++){fe iz2=fsqr(zinv[i]),ax=fmul(points[i].x,iz2),ay=fmul(points[i].y,fmul(zinv[i],iz2));pub[0]=(ay.v[0]&1)?3:2;be32(pub+1,ax);sha33(pub,sh);ripemd32(sh,rh);if(equal20(rh,target)){*found=(uint32_t)i;return 0;}}
 current=next;
 return -1;
}

void hash33_test(const uint8_t *in,uint8_t *out){uint8_t sh[32];sha33(in,sh);ripemd32(sh,out);}
void pubkey_test(const uint8_t *scalar,uint8_t *out){jac p=jmul(scalar);fe iz=fpow(p.z),iz2=fsqr(iz),x=fmul(p.x,iz2),y=fmul(p.y,fmul(iz,iz2));out[0]=(y.v[0]&1)?3:2;be32(out+1,x);}
