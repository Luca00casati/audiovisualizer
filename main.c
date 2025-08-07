#include <raylib.h>
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define FFT_SIZE 2048
#define MAX_BARS 256

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

typedef struct {
    float h, s, v;
} HSV;

HSV RGBtoHSV(Color c) {
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;

    float max = fmaxf(fmaxf(r, g), b);
    float min = fminf(fminf(r, g), b);
    float delta = max - min;

    HSV hsv;
    hsv.v = max;

    if (delta < 0.00001f) {
        hsv.h = 0;
        hsv.s = 0;
        return hsv;
    }

    if (max > 0.0f)
        hsv.s = delta / max;
    else {
        hsv.s = 0.0f;
        hsv.h = NAN;
        return hsv;
    }

    if (r >= max)
        hsv.h = (g - b) / delta;
    else if (g >= max)
        hsv.h = 2.0f + (b - r) / delta;
    else
        hsv.h = 4.0f + (r - g) / delta;

    hsv.h *= 60.0f;

    if (hsv.h < 0.0f)
        hsv.h += 360.0f;

    return hsv;
}

Color HSVtoRGB(HSV hsv) {
    float hh = hsv.h;
    if (hh >= 360.0f) hh = 0.0f;
    hh /= 60.0f;
    int i = (int)hh;
    float ff = hh - i;
    float p = hsv.v * (1.0f - hsv.s);
    float q = hsv.v * (1.0f - (hsv.s * ff));
    float t = hsv.v * (1.0f - (hsv.s * (1.0f - ff)));

    float r, g, b;
    switch(i) {
        case 0: r = hsv.v; g = t; b = p; break;
        case 1: r = q; g = hsv.v; b = p; break;
        case 2: r = p; g = hsv.v; b = t; break;
        case 3: r = p; g = q; b = hsv.v; break;
        case 4: r = t; g = p; b = hsv.v; break;
        case 5:
        default: r = hsv.v; g = p; b = q; break;
    }

    return (Color){(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 180};
}

bool IsDirectory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0) return false;
    return S_ISDIR(statbuf.st_mode);
}

bool IsAudioFile(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return false;
    if (strcasecmp(ext, ".wav") == 0 ||
        strcasecmp(ext, ".mp3") == 0 ||
        strcasecmp(ext, ".ogg") == 0) return true;
    return false;
}

char **GetAudioFilesInDir(const char *dirPath, int *count) {
    DIR *dir = opendir(dirPath);
    if (!dir) return NULL;

    char **fileList = NULL;
    int capacity = 10;
    int size = 0;
    fileList = malloc(capacity * sizeof(char*));

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && IsAudioFile(entry->d_name)) {
            if (size == capacity) {
                capacity *= 2;
                fileList = realloc(fileList, capacity * sizeof(char*));
            }
            int len = strlen(dirPath) + strlen(entry->d_name) + 2;
            char *fullpath = malloc(len);
            snprintf(fullpath, len, "%s/%s", dirPath, entry->d_name);
            fileList[size++] = fullpath;
        }
    }
    closedir(dir);
    *count = size;
    return fileList;
}

void VisualizeAudio(const char *audioPath, bool loop, const char *displayName, HSV baseHSV) {
    if (!FileExists(audioPath)) return;

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

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        int barCount = MAX_BARS;

        const float fixedBarWidth = 3.0f;
        float totalBarWidth = barCount * fixedBarWidth;
        float totalGapWidth = screenW - totalBarWidth;
        float gapWidth = totalGapWidth / (barCount + 1);

        float mags[MAX_BARS] = {0};
        float maxMag = 1.0f;

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
        ClearBackground(BLACK);
        BeginBlendMode(BLEND_ALPHA);

        HSV hsvColor = baseHSV;
        hsvColor.h += 10.0f * GetFrameTime();
        if (hsvColor.h >= 360.0f) hsvColor.h -= 360.0f;

        Color barColor = HSVtoRGB(hsvColor);

        for (int i = 0; i < barCount; i++) {
            float barHeight = smoothedInterp[i];
            float x = gapWidth + i * (fixedBarWidth + gapWidth);
            int y = screenH - barHeight;
            DrawRectangle((int)x, y, (int)fixedBarWidth, barHeight, barColor);
        }

        for (int i = 0; i < barCount; i++) {
            float x = gapWidth + i * (fixedBarWidth + gapWidth);
            int y = screenH - peaks[i];
            DrawRectangle((int)x, y, (int)fixedBarWidth, 4, WHITE);
        }

        EndBlendMode();
        DrawText(displayName, 20, 20, 40, WHITE);
        EndDrawing();

        baseHSV = hsvColor;
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

int main(int argc, char *argv[]) {
    bool loop = false;
    const char *path = NULL;

    if (argc < 2) {
        printf("Usage: %s [-l]<file_or_dir>\n", argv[0]);
        return 1;
    }

    if (argc >= 3 && strcmp(argv[1], "-l") == 0) {
        loop = true;
        path = argv[2];
    } else {
        path = argv[1];
    }

    InitWindow(1024, 600, "Visualizer");
    InitAudioDevice();

    HSV baseHSV = {210.0f, 0.7f, 0.8f}; // initial base color hue/saturation/value

    if (IsDirectory(path)) {
        int fileCount = 0;
        char **audioFiles = GetAudioFilesInDir(path, &fileCount);
        if (fileCount == 0) {
            printf("No audio files found in directory: %s\n", path);
            CloseAudioDevice();
            CloseWindow();
            return 1;
        }

        int currentFile = 0;
        do {
            const char *filename = strrchr(audioFiles[currentFile], '/');
            if (filename) filename++;
            else filename = audioFiles[currentFile];

            char audiostr[1024];
            snprintf(audiostr, sizeof(audiostr), "playing: %s", filename);

            VisualizeAudio(audioFiles[currentFile], false, audiostr, baseHSV);

            currentFile++;
            if (currentFile >= fileCount) currentFile = 0;

        } while (loop && !WindowShouldClose());

        for (int i = 0; i < fileCount; i++) free(audioFiles[i]);
        free(audioFiles);

    } else {
        if (!FileExists(path)) {
            CloseAudioDevice();
            CloseWindow();
            return 1;
        }
        const char *filename = strrchr(path, '/');
        if (filename) filename++;
        else filename = path;

        char audiostr[1024];
        snprintf(audiostr, sizeof(audiostr), "playing: %s", filename);
        VisualizeAudio(path, loop, audiostr, baseHSV);
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}

