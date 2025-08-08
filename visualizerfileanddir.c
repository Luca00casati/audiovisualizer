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
        draw_visualizer();
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
