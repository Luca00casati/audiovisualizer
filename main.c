#include "raylib.h"
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define FFT_SIZE 1024
#define MAX_BARS 128

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Color GradientColor(float t) {
    unsigned char r = (unsigned char)(255 * t);
    unsigned char g = (unsigned char)(255 * (1.0f - t));
    return (Color){r, g, 255, 180};
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;
    const char *audioPath = argv[1];
    char audiostr[1024];
    sprintf(audiostr, "playing: %s", audioPath);

    InitWindow(1024, 600, "Visualizer");
    InitAudioDevice();

    if (!FileExists(audioPath)) {
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    Sound sound = LoadSound(audioPath);
    Wave wave = LoadWave(audioPath);
    float *samples = LoadWaveSamples(wave);
    int totalSamples = wave.frameCount;

    fftw_complex *out = fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    double *in = fftw_malloc(sizeof(double) * FFT_SIZE);
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_ESTIMATE);

    float smoothed[MAX_BARS] = {0};
    float smoothedInterp[MAX_BARS] = {0};

    PlaySound(sound);
    int sampleCursor = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        for (int i = 0; i < FFT_SIZE; i++)
            in[i] = (sampleCursor + i < totalSamples) ? samples[sampleCursor + i] : 0.0;

        fftw_execute(plan);
        sampleCursor += FFT_SIZE;
        if (sampleCursor + FFT_SIZE >= totalSamples) {
            sampleCursor = 0;
            StopSound(sound);
            PlaySound(sound);
        }

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int barCount = MAX_BARS;

        float totalSpacing = (barCount - 1) * 2.0f;
        float barWidth = (screenW - totalSpacing) / (float)barCount;
        float spacing = 2.0f;

        float maxMag = 1.0f;
        float mags[MAX_BARS] = {0};

        for (int i = 0; i < barCount; i++) {
            float logMin = 0.0f;
            float logMax = log10f((float)(FFT_SIZE / 2));

            int startBin = (int)powf(10, lerp(logMin, logMax, (float)i / barCount));
            int endBin = (int)powf(10, lerp(logMin, logMax, (float)(i + 1) / barCount));
            if (startBin < 0) startBin = 0;
            if (endBin > FFT_SIZE / 2) endBin = FFT_SIZE / 2;
            if (endBin <= startBin) endBin = startBin + 1;

            float magSum = 0;
            int count = 0;
            for (int b = startBin; b < endBin; b++) {
                float re = out[b][0];
                float im = out[b][1];
                magSum += sqrtf(re * re + im * im);
                count++;
            }
            float mag = (count > 0) ? magSum / count : 0;
            mags[i] = mag;
            if (mag > maxMag) maxMag = mag;
        }

        for (int i = 0; i < barCount; i++) {
            float scaled = mags[i] / maxMag * screenH;
            smoothed[i] = lerp(smoothed[i], scaled, 0.2f);
        }

        for (int i = 0; i < barCount; i++) {
            float left = (i == 0) ? smoothed[i] : smoothed[i - 1];
            float right = (i == barCount - 1) ? smoothed[i] : smoothed[i + 1];
            smoothedInterp[i] = (left + smoothed[i] + right) / 3.0f;
        }

        BeginDrawing();
        ClearBackground(Fade(BLACK, 0.85f));
        BeginBlendMode(BLEND_ALPHA);

        for (int i = 0; i < barCount; i++) {
            float barHeight = smoothedInterp[i];
            float x = i * (barWidth + spacing);
            int y = screenH - barHeight;
            Color c = GradientColor((float)i / barCount);
            DrawRectangle((int)x, y, (int)barWidth, barHeight, c);
        }

        EndBlendMode();
        DrawText(audiostr, 20, 20, 40, WHITE);
        EndDrawing();
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    UnloadSound(sound);
    UnloadWave(wave);
    UnloadWaveSamples(samples);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

