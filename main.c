#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <fftw3.h>
#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include "common.h"

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
