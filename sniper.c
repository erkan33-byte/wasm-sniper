/* 4-weg SIMD-hash (vector extensions: gcc/clang, wasm met -msimd128).
 * P2PKH key scanner (compressed), secp256k1, affine "centrum +/- j*G" met gedeelde batch-inversie.
 * Per blok van NB=2*HALF+1 sleutels: 1 inversie-keten (3 mul per j) + ~4,5 mul per sleutel. */
#include <stdint.h>
#include <stddef.h>
#ifndef NATIVE
void *memset(void *s,int c,size_t n){unsigned char *p=s;while(n--)*p++=(unsigned char)c;return s;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}
#endif
#define HALF 512
#define NB (2*HALF+1)
#define C33 0x1000003D1ULL
typedef unsigned __int128 u128;
typedef struct{uint64_t v[4];}fe;
typedef struct{fe x,y,z;int inf;}jac;
static const fe GX={{0x59F2815B16F81798ULL,0x029BFCDB2DCE28D9ULL,0x55A06295CE870B07ULL,0x79BE667EF9DCBBACULL}};
static const fe GY={{0x9C47D08FFB10D4B8ULL,0xFD17B448A6855419ULL,0x5DA4FBFC0E1108A8ULL,0x483ADA7726A3C465ULL}};

static inline int ge_p(const uint64_t*r){return r[3]==~0ULL&&r[2]==~0ULL&&r[1]==~0ULL&&r[0]>=0xFFFFFFFEFFFFFC2FULL;}
static inline uint64_t addc33(uint64_t*r){u128 c=(u128)r[0]+C33;r[0]=(uint64_t)c;c>>=64;c+=r[1];r[1]=(uint64_t)c;c>>=64;c+=r[2];r[2]=(uint64_t)c;c>>=64;c+=r[3];r[3]=(uint64_t)c;return (uint64_t)(c>>64);}
static inline void subc33(uint64_t*r){uint64_t x=r[0],b;r[0]=x-C33;b=x<C33;for(int i=1;i<4;i++){x=r[i];r[i]=x-b;b=x<b;}}
static fe fred(const uint64_t t[8]){
 uint64_t r[4];u128 c=0;
 for(int i=0;i<4;i++){c+=(u128)t[4+i]*C33+t[i];r[i]=(uint64_t)c;c>>=64;}
 u128 d=(u128)(uint64_t)c*C33;
 c=(u128)r[0]+(uint64_t)d;r[0]=(uint64_t)c;c>>=64;
 c+=(u128)r[1]+(uint64_t)(d>>64);r[1]=(uint64_t)c;c>>=64;
 c+=r[2];r[2]=(uint64_t)c;c>>=64;
 c+=r[3];r[3]=(uint64_t)c;c>>=64;
 if(c)addc33(r);else if(ge_p(r))addc33(r);
 fe f={{r[0],r[1],r[2],r[3]}};return f;}
static fe fmul(fe a,fe b){uint64_t t[8]={0};
 for(int i=0;i<4;i++){u128 c=0;for(int j=0;j<4;j++){c+=(u128)a.v[i]*b.v[j]+t[i+j];t[i+j]=(uint64_t)c;c>>=64;}t[i+4]=(uint64_t)c;}
 return fred(t);}
static fe fsqr(fe a){return fmul(a,a);}
static fe fadd(fe a,fe b){u128 c=0;uint64_t r[4];for(int i=0;i<4;i++){c+=(u128)a.v[i]+b.v[i];r[i]=(uint64_t)c;c>>=64;}
 if(c)addc33(r);else if(ge_p(r))addc33(r);fe f={{r[0],r[1],r[2],r[3]}};return f;}
static fe fsub(fe a,fe b){uint64_t r[4],br=0;for(int i=0;i<4;i++){uint64_t x=a.v[i],y=b.v[i],d=x-y;uint64_t b1=x<y;uint64_t d2=d-br;uint64_t b2=d<br;r[i]=d2;br=b1|b2;}
 if(br)subc33(r);fe f={{r[0],r[1],r[2],r[3]}};return f;}
static fe fdouble(fe a){return fadd(a,a);}
static fe ftriple(fe a){return fadd(fdouble(a),a);}
static fe feight(fe a){a=fdouble(a);a=fdouble(a);return fdouble(a);}
static int fzero(fe a){return !(a.v[0]|a.v[1]|a.v[2]|a.v[3]);}
static fe fsqrn(fe a,int n){for(int i=0;i<n;i++)a=fsqr(a);return a;}
static fe finv(fe a){ /* a^(p-2) */
 fe x1=a,x2=fmul(fsqrn(x1,1),x1),x3=fmul(fsqrn(x2,1),x1),x6=fmul(fsqrn(x3,3),x3),x9=fmul(fsqrn(x6,3),x3),x11=fmul(fsqrn(x9,2),x2);
 fe x22=fmul(fsqrn(x11,11),x11),x44=fmul(fsqrn(x22,22),x22),x88=fmul(fsqrn(x44,44),x44),r=fmul(fsqrn(x88,88),x88);
 r=fmul(fsqrn(r,44),x44);r=fmul(fsqrn(r,3),x3);r=fmul(fsqrn(r,23),x22);r=fmul(fsqrn(r,5),x1);r=fmul(fsqrn(r,3),x2);r=fsqrn(r,2);return fmul(r,a);}

static jac jbase(void){jac p={GX,GY,{{1,0,0,0}},0};return p;}
static jac jdouble(jac p){if(p.inf||fzero(p.y)){p.inf=1;return p;}fe A=fsqr(p.x),B=fsqr(p.y),C=fsqr(B);fe D=fdouble(fsub(fsqr(fadd(p.x,B)),fadd(A,C)));fe E=ftriple(A),F=fsqr(E);jac r;r.x=fsub(F,fdouble(D));r.y=fsub(fmul(E,fsub(D,r.x)),feight(C));r.z=fdouble(fmul(p.y,p.z));r.inf=0;return r;}
static jac jaddg(jac p){if(p.inf)return jbase();fe z2=fsqr(p.z),u2=fmul(GX,z2),s2=fmul(GY,fmul(p.z,z2));fe h=fsub(u2,p.x),rr=fsub(s2,p.y);if(fzero(h)){p.inf=1;return p;}fe hh=fsqr(h),hhh=fmul(h,hh),v=fmul(p.x,hh);jac q;q.x=fsub(fsub(fsqr(rr),hhh),fdouble(v));q.y=fsub(fmul(rr,fsub(v,q.x)),fmul(p.y,hhh));q.z=fmul(p.z,h);q.inf=0;return q;}
static jac jmul(const uint8_t k[32]){jac r={{{0,0,0,0}},{{0,0,0,0}},{{0,0,0,0}},1};for(int i=0;i<256;i++){if(!r.inf)r=jdouble(r);if((k[i>>3]>>(7-(i&7)))&1)r=jaddg(r);}return r;}
static void to_affine(jac p,fe*x,fe*y){fe iz=finv(p.z),iz2=fsqr(iz);*x=fmul(p.x,iz2);*y=fmul(p.y,fmul(iz,iz2));}

static jac TJ[HALF+1];
static fe TX[HALF+1],TY[HALF+1],PRE[HALF+1],INV[HALF+1],DEN[HALF+1];
static fe CX,CY,SX,SY;static int built;
static void build(void){
 TJ[1]=jbase();TJ[2]=jdouble(TJ[1]);for(int j=3;j<=HALF;j++)TJ[j]=jaddg(TJ[j-1]);
 fe acc={{1,0,0,0}};for(int j=1;j<=HALF;j++){PRE[j]=acc;acc=fmul(acc,TJ[j].z);}
 acc=finv(acc);
 for(int j=HALF;j>=1;j--){fe zi=fmul(acc,PRE[j]);acc=fmul(acc,TJ[j].z);fe zi2=fsqr(zi);TX[j]=fmul(TJ[j].x,zi2);TY[j]=fmul(TJ[j].y,fmul(zi,zi2));}
 uint8_t k[32]={0};k[30]=NB>>8;k[31]=NB&255;jac s=jmul(k);to_affine(s,&SX,&SY);built=1;}
static void advance(void){fe dx=fsub(SX,CX),dy=fsub(SY,CY),l=fmul(dy,finv(dx));fe x3=fsub(fsub(fsqr(l),CX),SX);fe y3=fsub(fmul(l,fsub(CX,x3)),CY);CX=x3;CY=y3;}

static void be32(uint8_t out[32],fe x){for(int i=0;i<4;i++)for(int j=0;j<8;j++)out[31-(i*8+j)]=(uint8_t)(x.v[i]>>(j*8));}
typedef uint32_t v4 __attribute__((vector_size(16)));
#define SP(x) ((v4){(x),(x),(x),(x)})
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define ROL(x,n) (((x)<<(n))|((x)>>(32-(n))))
static const uint32_t K256[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static const uint8_t RL[80]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13};
static const uint8_t RR[80]={5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,15,5,1,3,7,14,6,9,11,8,12,2,10,0,4,13,8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11};
static const uint8_t SL[80]={11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6};
static const uint8_t SR[80]={8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11};
static inline v4 bs(v4 x){return (x>>24)|((x>>8)&0xff00)|((x<<8)&0xff0000)|(x<<24);}
/* SHA-256 + RIPEMD-160 voor 4 compressed pubkeys (33 bytes) tegelijk. h[i] lane l = LE-woord i van hash160 van sleutel l. */
static void hash4(const uint8_t pub[4][33],v4 h[5]){
 v4 w[16],s[8];
 for(int i=0;i<8;i++){v4 t;for(int l=0;l<4;l++)t[l]=((uint32_t)pub[l][i*4]<<24)|((uint32_t)pub[l][i*4+1]<<16)|((uint32_t)pub[l][i*4+2]<<8)|pub[l][i*4+3];w[i]=t;}
 {v4 t;for(int l=0;l<4;l++)t[l]=((uint32_t)pub[l][32]<<24)|0x00800000;w[8]=t;}
 for(int i=9;i<15;i++)w[i]=SP(0);w[15]=SP(264);
 v4 a=SP(0x6a09e667),b=SP(0xbb67ae85),c=SP(0x3c6ef372),d=SP(0xa54ff53a),e=SP(0x510e527f),f=SP(0x9b05688c),g=SP(0x1f83d9ab),hh=SP(0x5be0cd19);
 for(int i=0;i<64;i++){v4 v;
  if(i<16)v=w[i];else{v4 p15=w[(i-15)&15],p2=w[(i-2)&15];v=w[i&15]+(ROR(p15,7)^ROR(p15,18)^(p15>>3))+w[(i-7)&15]+(ROR(p2,17)^ROR(p2,19)^(p2>>10));w[i&15]=v;}
  v4 t1=hh+(ROR(e,6)^ROR(e,11)^ROR(e,25))+((e&f)^(~e&g))+K256[i]+v,t2=(ROR(a,2)^ROR(a,13)^ROR(a,22))+((a&b)^(a&c)^(b&c));
  hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
 s[0]=a+0x6a09e667;s[1]=b+0xbb67ae85;s[2]=c+0x3c6ef372;s[3]=d+0xa54ff53a;s[4]=e+0x510e527f;s[5]=f+0x9b05688c;s[6]=g+0x1f83d9ab;s[7]=hh+0x5be0cd19;
 for(int i=0;i<8;i++)w[i]=bs(s[i]);
 w[8]=SP(0x80);for(int i=9;i<14;i++)w[i]=SP(0);w[14]=SP(256);w[15]=SP(0);
 v4 al=SP(0x67452301),bl=SP(0xefcdab89),cl=SP(0x98badcfe),dl=SP(0x10325476),el=SP(0xc3d2e1f0),ar=al,br=bl,cr=cl,dr=dl,er=el;
 for(int j=0;j<80;j++){int r=j>>4;
  uint32_t kl=r==0?0:r==1?0x5a827999:r==2?0x6ed9eba1:r==3?0x8f1bbcdc:0xa953fd4e,kr=r==0?0x50a28be6:r==1?0x5c4dd124:r==2?0x6d703ef3:r==3?0x7a6d76e9:0;
  v4 fL,fR;
  if(r==0){fL=bl^cl^dl;fR=br^(cr|~dr);}else if(r==1){fL=(bl&cl)|(~bl&dl);fR=(br&dr)|(cr&~dr);}
  else if(r==2){fL=(bl|~cl)^dl;fR=(br|~cr)^dr;}else if(r==3){fL=(bl&dl)|(cl&~dl);fR=(br&cr)|(~br&dr);}else{fL=bl^(cl|~dl);fR=br^cr^dr;}
  int sl=SL[j],sr=SR[j];v4 x=al+fL+w[RL[j]]+kl;v4 t=((x<<sl)|(x>>(32-sl)))+el;al=el;el=dl;dl=ROL(cl,10);cl=bl;bl=t;
  x=ar+fR+w[RR[j]]+kr;t=((x<<sr)|(x>>(32-sr)))+er;ar=er;er=dr;dr=ROL(cr,10);cr=br;br=t;}
 h[0]=SP(0xefcdab89U)+cl+dr;h[1]=SP(0x98badcfeU)+dl+er;h[2]=SP(0x10325476U)+el+ar;h[3]=SP(0xc3d2e1f0U)+al+br;h[4]=SP(0x67452301U)+bl+cr;}

static fe QX[4],QY[4];static int QO[4],qn,GCOUNT;static uint32_t GT[5],*GFOUND;
static int flush(void){
 uint8_t pub[4][33];int n=qn;qn=0;
 for(int l=0;l<4;l++){int k=l<n?l:n-1;pub[l][0]=(QY[k].v[0]&1)?3:2;be32(pub[l]+1,QX[k]);}
 v4 h[5];hash4(pub,h);
 for(int l=0;l<n;l++)if(h[0][l]==GT[0]&&h[1][l]==GT[1]&&h[2][l]==GT[2]&&h[3][l]==GT[3]&&h[4][l]==GT[4]&&QO[l]<GCOUNT){*GFOUND=(uint32_t)QO[l];return 1;}
 return 0;}
static inline int push(fe x,fe y,int off){QX[qn]=x;QY[qn]=y;QO[qn]=off;return ++qn==4?flush():0;}
int batch_keys(void){return NB;}
void init_sniper(const uint8_t*start){
 if(!built)build();
 uint8_t k[32];for(int i=0;i<32;i++)k[i]=start[i];
 unsigned c=HALF;for(int i=31;i>=0&&c;i--){c+=k[i];k[i]=(uint8_t)c;c>>=8;}
 jac p=jmul(k);to_affine(p,&CX,&CY);}
/* Scant de sleutels start..start+NB-1 van het huidige blok (offset 0 = start). 0 = match (offset in *found), -1 = geen. */
int scan_batch(const uint8_t*target,int count,uint32_t*found){
 if(count<1)return -1;
 for(int i=0;i<5;i++)GT[i]=(uint32_t)target[i*4]|((uint32_t)target[i*4+1]<<8)|((uint32_t)target[i*4+2]<<16)|((uint32_t)target[i*4+3]<<24);
 GCOUNT=count;GFOUND=found;qn=0;
 for(int j=1;j<=HALF;j++){fe d=fsub(TX[j],CX);if(fzero(d)){d.v[0]=1;}DEN[j]=d;}
 fe acc={{1,0,0,0}};for(int j=1;j<=HALF;j++){PRE[j]=acc;acc=fmul(acc,DEN[j]);}
 acc=finv(acc);
 for(int j=HALF;j>=1;j--){INV[j]=fmul(acc,PRE[j]);acc=fmul(acc,DEN[j]);}
 if(push(CX,CY,HALF))return 0;
 for(int j=1;j<=HALF;j++){
  fe xj=TX[j],yj=TY[j],inv=INV[j],sx=fadd(CX,xj);
  fe l=fmul(fsub(yj,CY),inv),x3=fsub(fsqr(l),sx),y3=fsub(fmul(l,fsub(CX,x3)),CY);
  if(push(x3,y3,HALF+j))return 0;
  l=fmul(fsub(fsub(CY,CY),fadd(yj,CY)),inv);x3=fsub(fsqr(l),sx);y3=fsub(fmul(l,fsub(CX,x3)),CY);
  if(push(x3,y3,HALF-j))return 0;}
 if(qn&&flush())return 0;
 if(count>=NB)advance();
 return -1;}
