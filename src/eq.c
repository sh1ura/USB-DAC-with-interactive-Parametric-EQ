#include <stdio.h>
#include <math.h>
#include "eq.h"

static Complex add(Complex a, Complex b) {
  Complex c;

  c.re = a.re + b.re;
  c.im = a.im + b.im;
  return c;
}

static Complex sub(Complex a, Complex b) {
  Complex c;

  c.re = a.re - b.re;
  c.im = a.im - b.im;
  return c;
}

Complex mul(Complex a, Complex b) {
  Complex c;

  c.re = a.re * b.re - a.im * b.im;
  c.im = a.re * b.im + a.im * b.re;
  return c;
}

static Complex toComp(double a) {
  Complex c;

  c.re = a;
  c.im = 0;
  return c;
}

static Complex inv(Complex a) {
  Complex b;
  double d;
  
  b.re = a.re;
  b.im = -a.im;
  d = a.re * a.re + a.im * a.im;
  b.re /= d;
  b.im /= d;

  return b;
}

// calculate response of biquad filter
Complex calcResponse(BiQuadCoeffs c, double freq, double fs) {
  double om = 2.0 * M_PI *  freq / fs;
  Complex x, y, z;

  z.re = cos(om);
  z.im = sin(om);
  z = inv(z);

  x = toComp(c.b0);
  x = add(x, mul(toComp(c.b1), z));
  x = add(x, mul(toComp(c.b2), mul(z, z)));

  y = toComp(c.a0);
  y = add(y, mul(toComp(c.a1), z));
  y = add(y, mul(toComp(c.a2), mul(z, z)));

  return mul(x, inv(y));
}

BiQuadCoeffs calcCoeffs(double fs, double f0, double bw, double gain) {
  BiQuadCoeffs c;
  double omega = 2.0 * M_PI * f0 / fs;
  double alpha = sin(omega) * sinh(log(2.0) / 2.0 * bw * omega / sin(omega));
  double A     = pow(10.0, (gain / 40.0));

  c.a0 =  1.0 + alpha / A;
  c.a1 = -2.0 * cos(omega);
  c.a2 =  1.0 - alpha / A;
  c.b0 =  1.0 + alpha * A;
  c.b1 = -2.0 * cos(omega);
  c.b2 =  1.0 - alpha * A;

  c.b0 /= c.a0;
  c.b1 /= c.a0;
  c.b2 /= c.a0;
  c.a1 /= c.a0;
  c.a2 /= c.a0;
  c.a0 /= c.a0;
  
  return c;
}


  
