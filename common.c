#include <raylib.h>
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"

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


void draw_visualizer(HSV baseHSV, fftw_complex *out, float *smoothed,  float *smoothedInterp,  float *peaks, float avgMaxMag, const char *displayName){
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


