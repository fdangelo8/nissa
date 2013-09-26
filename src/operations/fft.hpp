#ifndef _FFT_H
#define _FFT_H

namespace nissa
{
  int bitrev(int in,int l2n);
  int find_max_pow2(int a);
  void data_coordinate_order_shift(complex *data,int ncpp,int mu0);
  void fft1d(complex *out,complex *in,int ncpp,int mu,double sign,int normalize);
  void fft4d(complex *out,complex *in,int ncpp,double sign,int normalize);
}

#endif