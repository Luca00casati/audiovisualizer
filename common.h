#ifndef COMMON_H
#define COMMON_H
#define FFT_SIZE 2048
#define MAX_BARS 256

void draw_visualizer(HSV baseHSV, fftw_complex *out, float *smoothed,  float *smoothedInterp,  float *peaks, float avgMaxMag, const char *displayName);
#endif
