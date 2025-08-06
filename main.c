#include <raylib.h>
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FFT_SIZE 2048
#define MAX_BARS 256

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Color GradientColor(float t) {
    unsigned char r = (unsigned char)(255 * t);
    unsigned char g = (unsigned char)(255 * (1.0f - t));
    return (Color){r, g, 255, 180};
}

int main(int argc, char *argv[]) {
    const char *audioPath = NULL;
    const char *filename = NULL;

    if (argc >= 2) {
        audioPath = argv[1];
        filename = strrchr(audioPath, '/');
        if (filename) filename++;
        else filename = audioPath;
    }

    InitWindow(1024, 600, "Visualizer");
    InitAudioDevice();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetTargetFPS(60);

    Music music = {0};
    Wave wave = {0};
    float *samples = NULL;
    int totalSamples = 0;
    char audiostr[1024] = "none";

    bool musicLoaded = false;

    if (audioPath && FileExists(audioPath)) {
        music = LoadMusicStream(audioPath);
        wave = LoadWave(audioPath);
        samples = LoadWaveSamples(wave);
        totalSamples = wave.frameCount;
        snprintf(audiostr, sizeof(audiostr), "playing: %s", filename);
        PlayMusicStream(music);
        musicLoaded = true;
    }

    fftw_complex *out = fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    double *in = fftw_malloc(sizeof(double) * FFT_SIZE);
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_ESTIMATE);

    float *smoothed = calloc(MAX_BARS, sizeof(float));
    float *smoothedInterp = calloc(MAX_BARS, sizeof(float));
    float *peaks = calloc(MAX_BARS, sizeof(float));

    static float avgMaxMag = 1.0f;

    while (!WindowShouldClose()) {
        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            if (dropped.count > 0) {
                if (musicLoaded) {
                    UnloadMusicStream(music);
                    UnloadWave(wave);
                    UnloadWaveSamples(samples);
                }

                audioPath = dropped.paths[0];
                filename = strrchr(audioPath, '/');
                if (filename) filename++;
                else filename = audioPath;

                if (FileExists(audioPath)) {
                    music = LoadMusicStream(audioPath);
                    wave = LoadWave(audioPath);
                    samples = LoadWaveSamples(wave);
                    totalSamples = wave.frameCount;
                    snprintf(audiostr, sizeof(audiostr), "playing: %s", filename);
                    PlayMusicStream(music);
                    musicLoaded = true;
                }

                UnloadDroppedFiles(dropped);
            }
        }

        if (musicLoaded) {
            UpdateMusicStream(music);
            unsigned int sampleCursor = GetMusicTimePlayed(music) * wave.sampleRate;

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

            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            int barCount = MAX_BARS;
            float spacing = 2.0f;
            float totalSpacing = (barCount - 1) * spacing;
            float barWidth = (screenW - totalSpacing) / (float)barCount;

            float maxMag = 1.0f;
            float mags[MAX_BARS] = {0};

            for (int i = 0; i < barCount; i++) {
                float logMin = log10f(2.0f);
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
                    float mag = sqrtf(re * re + im * im);
                    magSum += sqrtf(mag);
                    count++;
                }
                float mag = (count > 0) ? magSum / count : 0;
                mags[i] = mag;
                if (mag > maxMag) maxMag = mag;
            }

            avgMaxMag = lerp(avgMaxMag, maxMag, 0.1f);

            for (int i = 0; i < barCount; i++) {
                float scaled = mags[i] / avgMaxMag * screenH;
                smoothed[i] = lerp(smoothed[i], scaled, 0.2f);
            }

            for (int i = 0; i < barCount; i++) {
                float left = (i == 0) ? smoothed[i] : smoothed[i - 1];
                float right = (i == barCount - 1) ? smoothed[i] : smoothed[i + 1];
                smoothedInterp[i] = (left + smoothed[i] + right) / 3.0f;
            }

            for (int i = 0; i < barCount; i++) {
                if (smoothedInterp[i] > peaks[i])
                    peaks[i] = smoothedInterp[i];
                else
                    peaks[i] = lerp(peaks[i], smoothedInterp[i], 0.05f);
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

            for (int i = 0; i < barCount; i++) {
                float x = i * (barWidth + spacing);
                int y = screenH - peaks[i];
                DrawRectangle((int)x, y, (int)barWidth, 4, WHITE);
            }

            EndBlendMode();
            DrawText(audiostr, 20, 20, 40, WHITE);
            EndDrawing();
        } else {
            BeginDrawing();
            ClearBackground(Fade(BLACK, 0.85f));
            DrawText("none", 20, 20, 40, WHITE);
            EndDrawing();
        }
    }

    if (musicLoaded) {
        UnloadWaveSamples(samples);
        UnloadWave(wave);
        UnloadMusicStream(music);
    }

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    free(smoothed);
    free(smoothedInterp);
    free(peaks);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}

