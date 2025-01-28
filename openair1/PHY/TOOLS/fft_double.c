#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "tools_defs.h"


void twiddle(cd_t *W, int N, double stuff)
{
  W->r=cos(stuff*2.0*M_PI/(double)N);
  W->i=-sin(stuff*2.0*M_PI/(double)N);
}


int bitrev64[64] = {0,32,16,48,8,40,24,56,4,36,20,52,12,44,28,60,2,34,18,50,10,42,26,58,
6,38,22,54,14,46,30,62,1,33,17,49,9,41,25,57,5,37,21,53,13,45,29,61,
3,35,19,51,11,43,27,59,7,39,23,55,15,47,31,63};
int bitrev128[128];
int bitrev256[256];
int bitrev512[512];
int bitrev1024[1024];
int bitrev2048[2048];
int bitrev4096[4096];

void init_bitrev() {

  // 128
  for (int i=0;i<64;i++) { bitrev128[i]=2*bitrev64[i]; bitrev128[i+64]=1+bitrev128[i]; }

  // 256 
  for (int i=0;i<128;i++) { bitrev256[i]=2*bitrev128[i]; bitrev256[i+128]=1+bitrev256[i]; }
    
  // 512 
  for (int i=0;i<256;i++) { bitrev512[i]=2*bitrev256[i]; bitrev512[i+256]=1+bitrev512[i]; }

  // 1024 
  for (int i=0;i<512;i++) { bitrev1024[i]=2*bitrev512[i]; bitrev1024[i+512]=1+bitrev1024[i]; }

  // 2048 
  for (int i=0;i<1024;i++) { bitrev2048[i]=2*bitrev1024[i]; bitrev2048[i+1024]=1+bitrev2048[i]; }

  // 4096 
  for (int i=0;i<2048;i++) { bitrev4096[i]=2*bitrev2048[i]; bitrev4096[i+2048]=1+bitrev4096[i]; }

}

/** RADIX-2 FFT ALGORITHM */
/* Double precision*/
void radix2(cd_t *x, int N)
{
  int    n2, k1, N1, N2;
  cd_t W, bfly[2];

  N1=2;
  N2=N/2;
  /** Do 2 Point DFT */
  for (n2=0; n2<N2; n2++)
    {
      /** Radix 2 butterfly */
      bfly[0].r = (x[n2].r + x[N2 + n2].r);
      bfly[0].i = (x[n2].i + x[N2 + n2].i);

      bfly[1].r = (x[n2].r - x[N2 + n2].r);
      bfly[1].i = (x[n2].i - x[N2 + n2].i);



      twiddle(&W, N, (double)n2);
      x[n2].r = bfly[0].r;
      x[n2].i = bfly[0].i;
      x[n2 + N2].r = bfly[1].r*W.r - bfly[1].i*W.i;
      x[n2 + N2].i = bfly[1].i*W.r + bfly[1].r*W.i;
    }
 
  /** Don't recurse if we're down to one butterfly */
  if (N2!=1) {
	radix2(&x[0], N2);
	radix2(&x[N2], N2);
  }
}

void normalize(cd_t *x,cd_t *y, int *bitrev, int N) {
  for (int i=0;i<N;i++) {
    y[i].r = x[bitrev[i]].r / sqrt((double)N);
    y[i].i = x[bitrev[i]].i / sqrt((double)N);
  }
}



