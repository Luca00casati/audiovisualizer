#include <raylib.h>
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "common.h"

void VisualizeAudioFiles(const char *audioPath, bool loop, const char *displayName, HSV baseHSV) {
    if (!FileExists(audioPath)) return;
    bool isPaused = false;

    Music music = LoadMusicStream(audioPath);
    music.looping = loop;
    Wave wave = LoadWave(audioPath);
    float *samples = LoadWaveSamples(wave);
    int totalSamples = wave.frameCount;

    fftw_complex *out = fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    double *in = fftw_malloc(sizeof(double) * FFT_SIZE);
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_ESTIMATE);

    float *smoothed = calloc(MAX_BARS, sizeof(float));
    float *smoothedInterp = calloc(MAX_BARS, sizeof(float));
    float *peaks = calloc(MAX_BARS, sizeof(float));

    PlayMusicStream(music);
    SetTargetFPS(60);

    float avgMaxMag = 1.0f;

    while (!WindowShouldClose() && (loop || IsMusicStreamPlaying(music))) {
        if (IsKeyPressed(KEY_SPACE)) {
            isPaused = !isPaused;
            if (isPaused) PauseMusicStream(music);
            else ResumeMusicStream(music);
        }

        UpdateMusicStream(music);
        unsigned int sampleCursor = (unsigned int)(GetMusicTimePlayed(music) * wave.sampleRate);

        for (int i = 0; i < FFT_SIZE; i++) {
            if ((int)(sampleCursor + i) < totalSamples) {
                float left = samples[(sampleCursor + i) * wave.channels];
                float right = wave.channels > 1 ? samples[(sampleCursor + i) * wave.channels + 1] : left;
                in[i] = 0.5f * (left + right);
            } else {
                in[i] = 0.0;
            }
        }

        fftw_execute(plan);
        draw_visualizer(out, smoothed,  smoothedInterp,  peaks, &avgMaxMag, displayName, &baseHSV);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    free(smoothed);
    free(smoothedInterp);
    free(peaks);
    UnloadWaveSamples(samples);
    UnloadWave(wave);
    UnloadMusicStream(music);
}
