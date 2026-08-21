#include <stdint.h>
#include <stddef.h>

void *memset(void *s,int c,size_t n){unsigned char *p=s;while(n--)*p++=(unsigned char)c;return s;}
void *memcpy(void *d,const void *s,size_t n){unsigned char *a=d;const unsigned char *b=s;while(n--)*a++=*b++;return d;}

#define MAX_BABIES 262145u
#define HASH_CAP 524288u
#define BLOCK 10000u

typedef unsigned __int128 u128;
typedef struct { uint64_t v[4]; } fe;
typedef struct { fe x,y,z; int inf; } jac;
typedef struct { fe x,y; } aff;

static const fe FP={{0xFFFFFFFEFFFFFC2FULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL}};
static const fe GX={{0x59F2815B16F81798ULL,0x029BFCDB2DCE28D9ULL,0x55A06295CE870B07ULL,0x79BE667EF9DCBBACULL}};
static const fe GY={{0x9C47D08FFB10D4B8ULL,0xFD17B448A6855419ULL,0x5DA4FBFC0E1108A8ULL,0x483ADA7726A3C465ULL}};
static const uint64_t PM2[4]={0xFFFFFFFEFFFFFC2DULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL};
/* (p+1)/4, because p = 3 (mod 4). */
static const uint64_t SQRT_EXP[4]={0xFFFFFFFFBFFFFF0CULL,0xFFFFFFFFFFFFFFFFULL,0xFFFFFFFFFFFFFFFFULL,0x3FFFFFFFFFFFFFFFULL};

static int cmp4(const uint64_t *a,const uint64_t *b){for(int i=3;i>=0;i--){if(a[i]>b[i])return 1;if(a[i]<b[i])return -1;}return 0;}
static int feq(fe a,fe b){return a.v[0]==b.v[0]&&a.v[1]==b.v[1]&&a.v[2]==b.v[2]&&a.v[3]==b.v[3];}
static int fzero(fe a){return !(a.v[0]|a.v[1]|a.v[2]|a.v[3]);}
static void sub4(uint64_t *a,const uint64_t *b){uint64_t borrow=0;for(int i=0;i<4;i++){uint64_t x=a[i],y=b[i]+borrow;uint64_t c=(borrow?y<=b[i]:0);a[i]=x-y;borrow=(x<y)||c;}}
static void addwide(uint64_t *t,int p,uint64_t a){while(a&&p<10){u128 s=(u128)t[p]+a;t[p]=(uint64_t)s;a=(uint64_t)(s>>64);p++;}}
static void addwide128(uint64_t *t,int p,u128 a){addwide(t,p,(uint64_t)a);addwide(t,p+1,(uint64_t)(a>>64));}
static fe reduce10(uint64_t t[10]){for(int pass=0;pass<12;pass++)for(int i=9;i>=4;i--){uint64_t q=t[i];if(!q)continue;t[i]=0;addwide128(t,i-4,(u128)q*977);addwide128(t,i-4,(u128)q<<32);}fe r={{t[0],t[1],t[2],t[3]}};while(cmp4(r.v,FP.v)>=0)sub4(r.v,FP.v);return r;}
static fe fadd(fe a,fe b){uint64_t t[10]={0};for(int i=0;i<4;i++){u128 s=(u128)a.v[i]+b.v[i]+t[i];t[i]=(uint64_t)s;addwide(t,i+1,(uint64_t)(s>>64));}return reduce10(t);}
static fe fsub(fe a,fe b){if(cmp4(a.v,b.v)>=0){fe r=a;sub4(r.v,b.v);return r;}fe n=FP;sub4(n.v,b.v);return fadd(a,n);}
static fe fmul(fe a,fe b){uint64_t t[10]={0};for(int i=0;i<4;i++){u128 carry=0;for(int j=0;j<4;j++){u128 s=(u128)a.v[i]*b.v[j]+t[i+j]+carry;t[i+j]=(uint64_t)s;carry=s>>64;}addwide128(t,i+4,carry);}return reduce10(t);}
static fe fsqr(fe a){return fmul(a,a);}
static fe fdouble(fe a){return fadd(a,a);}
static fe ftriple(fe a){return fadd(fdouble(a),a);}
static fe feight(fe a){a=fdouble(a);a=fdouble(a);return fdouble(a);}
static fe fpowexp(fe a,const uint64_t e[4]){fe r={{1,0,0,0}};for(int i=255;i>=0;i--){r=fsqr(r);if((e[i>>6]>>(i&63))&1ULL)r=fmul(r,a);}return r;}
/* Vaste secp256k1-keten voor a^(p-2): 255 squares en 15 multiplies. */
static fe fsqrn(fe a,int n){for(int i=0;i<n;i++)a=fsqr(a);return a;}
static fe fpow(fe a){fe x1=a;fe x2=fmul(fsqrn(x1,1),x1);fe x3=fmul(fsqrn(x2,1),x1);fe x6=fmul(fsqrn(x3,3),x3);fe x9=fmul(fsqrn(x6,3),x3);fe x11=fmul(fsqrn(x9,2),x2);fe x22=fmul(fsqrn(x11,11),x11);fe x44=fmul(fsqrn(x22,22),x22);fe x88=fmul(fsqrn(x44,44),x44);fe r=fmul(fsqrn(x88,88),x88);r=fmul(fsqrn(r,44),x44);r=fmul(fsqrn(r,3),x3);r=fmul(fsqrn(r,23),x22);r=fmul(fsqrn(r,5),x1);r=fmul(fsqrn(r,3),x2);r=fsqrn(r,2);return fmul(r,a);}

static jac jbase(void){jac p={GX,GY,{{1,0,0,0}},0};return p;}
static jac jdouble(jac p){if(p.inf||fzero(p.y)){p.inf=1;return p;}fe a=fsqr(p.x),b=fsqr(p.y),c=fsqr(b);fe d=fdouble(fsub(fsqr(fadd(p.x,b)),fadd(a,c)));fe e=ftriple(a),f=fsqr(e);jac r;r.x=fsub(f,fdouble(d));r.y=fsub(fmul(e,fsub(d,r.x)),feight(c));r.z=fdouble(fmul(p.y,p.z));r.inf=0;return r;}
static jac jadd_aff(jac p,aff q){if(p.inf){jac r={q.x,q.y,{{1,0,0,0}},0};return r;}fe z2=fsqr(p.z),u2=fmul(q.x,z2),s2=fmul(q.y,fmul(p.z,z2));fe h=fsub(u2,p.x),rr=fsub(s2,p.y);if(fzero(h)){if(fzero(rr))return jdouble(p);p.inf=1;return p;}fe hh=fsqr(h),hhh=fmul(h,hh),v=fmul(p.x,hh);jac r;r.x=fsub(fsub(fsqr(rr),hhh),fdouble(v));r.y=fsub(fmul(rr,fsub(v,r.x)),fmul(p.y,hhh));r.z=fmul(p.z,h);r.inf=0;return r;}
static jac jaddg(jac p){aff g={GX,GY};return jadd_aff(p,g);}
static jac jneg(jac p){if(!p.inf&&!fzero(p.y))p.y=fsub(FP,p.y);return p;}
static jac jmul(const uint8_t k[32]){jac r={{{0,0,0,0}},{{0,0,0,0}},{{0,0,0,0}},1};for(int i=0;i<256;i++){if(!r.inf)r=jdouble(r);if((k[i>>3]>>(7-(i&7)))&1)r=jaddg(r);}return r;}
static jac jmul_u32(uint32_t k){uint8_t s[32]={0};s[28]=(uint8_t)(k>>24);s[29]=(uint8_t)(k>>16);s[30]=(uint8_t)(k>>8);s[31]=(uint8_t)k;return jmul(s);}

static jac work[BLOCK];
static fe prefix[BLOCK],invz[BLOCK];
static aff babies[MAX_BABIES];
static uint32_t slots[HASH_CAP];
static uint32_t cached_m=0;
static aff cached_minus_mg;
static uint8_t cached_pub[33];
static aff cached_target;
static int cached_target_valid=0;

static void batch_invert(uint32_t n){fe acc={{1,0,0,0}};for(uint32_t i=0;i<n;i++){prefix[i]=acc;if(!work[i].inf)acc=fmul(acc,work[i].z);}acc=fpow(acc);for(uint32_t ii=n;ii>0;ii--){uint32_t i=ii-1;if(work[i].inf){invz[i]=(fe){{0,0,0,0}};}else{invz[i]=fmul(acc,prefix[i]);acc=fmul(acc,work[i].z);}}}
static aff to_aff(uint32_t i){fe iz2=fsqr(invz[i]);aff a;a.x=fmul(work[i].x,iz2);a.y=fmul(work[i].y,fmul(invz[i],iz2));return a;}
static uint32_t mix_x(fe x){uint64_t z=x.v[0]^x.v[1]^x.v[2]^x.v[3];z^=z>>33;z*=0xff51afd7ed558ccdULL;z^=z>>33;z*=0xc4ceb9fe1a85ec53ULL;z^=z>>33;return (uint32_t)(z^(z>>32));}
static void table_insert(uint32_t index){uint32_t p=mix_x(babies[index].x)&(HASH_CAP-1u);while(slots[p])p=(p+1u)&(HASH_CAP-1u);slots[p]=index+1u;}
static int table_find(aff a,uint32_t *out){uint32_t p=mix_x(a.x)&(HASH_CAP-1u);for(uint32_t n=0;n<HASH_CAP;n++){uint32_t v=slots[p];if(!v)return 0;uint32_t i=v-1u;if(feq(babies[i].x,a.x)){*out=i;return 1;}p=(p+1u)&(HASH_CAP-1u);}return 0;}

/* Build and cache {G,2G,...,mG}; m must be <= 262145. */
int bsgs_prepare(uint32_t m){if(!m||m>MAX_BABIES)return -1;if(cached_m==m)return 0;cached_m=0;memset(slots,0,sizeof(slots));jac p=jbase();uint32_t off=0;while(off<m){uint32_t n=m-off;if(n>BLOCK)n=BLOCK;for(uint32_t i=0;i<n;i++){work[i]=p;p=jaddg(p);}batch_invert(n);for(uint32_t i=0;i<n;i++)babies[off+i]=to_aff(i);off+=n;}for(uint32_t i=0;i<m;i++)table_insert(i);/* De laatste baby is exact mG; hergebruik die affine waarde in plaats van een extra scalar multiplication. */cached_minus_mg=babies[m-1u];if(!fzero(cached_minus_mg.y))cached_minus_mg.y=fsub(FP,cached_minus_mg.y);cached_m=m;return 0;}

static int decompress(const uint8_t pub[33],aff *out){if(pub[0]!=2&&pub[0]!=3)return 0;fe x={{0,0,0,0}};for(int i=0;i<32;i++)x.v[(31-i)>>3]|=(uint64_t)pub[i+1]<<(((31-i)&7)*8);if(cmp4(x.v,FP.v)>=0)return 0;fe rhs=fadd(fmul(fsqr(x),x),(fe){{7,0,0,0}});fe y=fpowexp(rhs,SQRT_EXP);if(!feq(fsqr(y),rhs))return 0;if((y.v[0]&1ULL)!=(uint64_t)(pub[0]&1))y=fsub(FP,y);out->x=x;out->y=y;return 1;}
static int cmpbe(const uint8_t *a,const uint8_t *b){for(int i=0;i<32;i++){if(a[i]>b[i])return 1;if(a[i]<b[i])return -1;}return 0;}
static int bytes_eq(const uint8_t *a,const uint8_t *b,uint32_t n){for(uint32_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static int cached_decompress(const uint8_t pub[33],aff *out){if(!cached_target_valid||!bytes_eq(pub,cached_pub,33)){if(!decompress(pub,&cached_target))return 0;memcpy(cached_pub,pub,33);cached_target_valid=1;}*out=cached_target;return 1;}
static void add_be_u64(const uint8_t in[32],uint64_t add,uint8_t out[32]){memcpy(out,in,32);uint64_t carry=add;for(int i=31;i>=0&&carry;i--){uint64_t s=(uint64_t)out[i]+(carry&255ULL);out[i]=(uint8_t)s;carry=(carry>>8)+(s>>8);}}
static int candidate(const uint8_t start[32],const uint8_t end[32],uint64_t delta,uint8_t out[32]){add_be_u64(start,delta,out);return cmpbe(out,end)<=0;}

/* Returns 1 for match, 0 for no match, negative for invalid input. giants = ceil((end-start+1)/m). */
int bsgs_scan(const uint8_t pub[33],const uint8_t start[32],const uint8_t end[32],uint32_t m,uint32_t giants,uint8_t out[32]){
 if(cmpbe(start,end)>0||!m||!giants)return -1;if(bsgs_prepare(m))return -2;aff target;if(!cached_decompress(pub,&target))return -3;
 jac s=jmul(start);jac q=jadd_aff(jneg(s),target);if(q.inf){memcpy(out,start,32);return 1;}
 uint32_t gbase=0;while(gbase<giants){uint32_t n=giants-gbase;if(n>BLOCK)n=BLOCK;for(uint32_t i=0;i<n;i++){work[i]=q;q=jadd_aff(q,cached_minus_mg);}batch_invert(n);for(uint32_t i=0;i<n;i++){if(work[i].inf)continue;aff a=to_aff(i);uint32_t bi;if(!table_find(a,&bi))continue;uint64_t base=(uint64_t)(gbase+i)*(uint64_t)m;if(feq(babies[bi].y,a.y)){uint64_t d=base+(uint64_t)bi+1ULL;if(candidate(start,end,d,out))return 1;}else{fe ny=fsub(FP,babies[bi].y);if(feq(ny,a.y)&&base>=(uint64_t)bi+1ULL){uint64_t d=base-((uint64_t)bi+1ULL);if(candidate(start,end,d,out))return 1;}}}gbase+=n;}return 0;
}

