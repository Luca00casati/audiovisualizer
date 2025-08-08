#ifndef COMMON_H
#define COMMON_H
#define FFT_SIZE 2048
#define MAX_BARS 256

typedef struct {
    float h, s, v;
} HSV;

Color HSVtoRGB(HSV hsv);
HSV RGBtoHSV(Color c);
float lerp(float a, float b, float t);
bool IsDirectory(const char *path);
bool IsAudioFile(const char *filename);
char **GetAudioFilesInDir(const char *dirPath, int *count);
void VisualizeAudioFiles(const char *audioPath, bool loop, const char *displayName, HSV baseHSV);
void VisualizePulseAudio(const char * src, HSV baseHSV);
void draw_visualizer(fftw_complex *out, float *smoothed,  float *smoothedInterp,  float *peaks, float* avgMaxMag, const char *displayName, HSV* baseHSV);
#endif
