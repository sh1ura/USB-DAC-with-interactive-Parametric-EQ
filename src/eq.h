typedef struct {
  double re, im;
} Complex;

typedef struct {
  double a0, a1, a2, b0, b1, b2;
} BiQuadCoeffs;

BiQuadCoeffs calcCoeffs(double fs, double f0, double bw, double gain);
Complex calcResponse(BiQuadCoeffs c, double freq, double fs);
Complex mul(Complex, Complex);
