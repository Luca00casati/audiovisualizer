#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <fftw3.h>
#include <raylib.h>
#include <math.h>
#include <stdlib.h>

#define FFT_SIZE 2048
#define MAX_BARS 256

static pa_mainloop *pa_ml = NULL;
static pa_context *pa_ctx = NULL;
static pa_stream *pa_stream_ptr = NULL;

static double in[FFT_SIZE];
static fftw_complex *out = NULL;
static fftw_plan plan;

static float smoothed[MAX_BARS];
static float smoothedInterp[MAX_BARS];
static float peaks[MAX_BARS];

static bool ready = false;
static bool quit = false;

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

void cleanup_pulse() {
    if (pa_stream_ptr) {
        pa_stream_disconnect(pa_stream_ptr);
        pa_stream_unref(pa_stream_ptr);
        pa_stream_ptr = NULL;
    }
    if (pa_ctx) {
        pa_context_disconnect(pa_ctx);
        pa_context_unref(pa_ctx);
        pa_ctx = NULL;
    }
    if (pa_ml) {
        pa_mainloop_free(pa_ml);
        pa_ml = NULL;
    }
}

void stream_read_cb(pa_stream *s, size_t length, void *userdata) {
    const void *data;
    if (pa_stream_peek(s, &data, &length) < 0) {
        return;
    }
    if (!data) {
        pa_stream_drop(s);
        return;
    }

    const int16_t *samples = (const int16_t *)data;
    int frames = length / (sizeof(int16_t) * 2); // 2 channels

    static int sampleIndex = 0;

    for (int i = 0; i < frames && sampleIndex < FFT_SIZE; i++) {
        float left = samples[i * 2] / 32768.0f;
        float right = samples[i * 2 + 1] / 32768.0f;
        in[sampleIndex++] = 0.5f * (left + right);
    }

    if (sampleIndex >= FFT_SIZE) {
        sampleIndex = 0;
        fftw_execute(plan);
    }

    pa_stream_drop(s);
}

void context_state_cb(pa_context *c, void *userdata) {
    pa_context_state_t state = pa_context_get_state(c);
    (void)userdata;
    switch (state) {
        case PA_CONTEXT_READY:
            ready = true;
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            quit = true;
            break;
        default:
            break;
    }
}

void VisualizePulseAudio() {
    InitWindow(1024, 600, "PulseAudio Visualizer");
    InitAudioDevice();

    for (int i = 0; i < MAX_BARS; i++) smoothed[i] = 0;
    for (int i = 0; i < MAX_BARS; i++) smoothedInterp[i] = 0;
    for (int i = 0; i < MAX_BARS; i++) peaks[i] = 0;

    out = fftw_malloc(sizeof(fftw_complex) * FFT_SIZE);
    plan = fftw_plan_dft_r2c_1d(FFT_SIZE, in, out, FFTW_ESTIMATE);

    pa_ml = pa_mainloop_new();
    pa_mainloop_api *pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "PulseAudio Visualizer");
    pa_context_set_state_callback(pa_ctx, context_state_cb, NULL);
    pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL);

    while (!ready && !quit) {
        pa_mainloop_iterate(pa_ml, 1, NULL);
    }
    if (quit) {
        cleanup_pulse();
        CloseAudioDevice();
        CloseWindow();
        return;
    }

    pa_sample_spec sample_spec = {
        .format = PA_SAMPLE_S16LE,
        .channels = 2,
        .rate = 48000,
    };

    pa_stream_ptr = pa_stream_new(pa_ctx, "Record Stream", &sample_spec, NULL);
    pa_stream_set_read_callback(pa_stream_ptr, stream_read_cb, NULL);

    int ret = pa_stream_connect_record(pa_stream_ptr, "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor", NULL, PA_STREAM_ADJUST_LATENCY);
    if (ret < 0) {
        printf("Failed to connect record stream\n");
        cleanup_pulse();
        CloseAudioDevice();
        CloseWindow();
        return;
    }

    float avgMaxMag = 1.0f;

    SetTargetFPS(60);
    while (!WindowShouldClose() && !quit) {
        pa_mainloop_iterate(pa_ml, 0, NULL);

        int barCount = MAX_BARS;
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

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

        for (int i = 0; i < barCount; i++) {
            float barHeight = smoothedInterp[i];
            float x = gapWidth + i * (fixedBarWidth + gapWidth);
            int y = screenH - barHeight;
            DrawRectangle((int)x, y, (int)fixedBarWidth, barHeight, BLUE);
        }

        for (int i = 0; i < barCount; i++) {
            float x = gapWidth + i * (fixedBarWidth + gapWidth);
            int y = screenH - peaks[i];
            DrawRectangle((int)x, y, (int)fixedBarWidth, 4, WHITE);
        }

        EndDrawing();
    }

    fftw_destroy_plan(plan);
    fftw_free(out);
    cleanup_pulse();

    CloseAudioDevice();
    CloseWindow();
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "-p") == 0) {
        VisualizePulseAudio();
    } else {
        printf("Usage: %s -p\n", argv[0]);
        return 1;
    }
    return 0;
}

