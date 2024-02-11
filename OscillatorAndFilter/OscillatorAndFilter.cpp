#include <stdio.h>
#include <math.h>
#include <vector>
#include <unordered_map>
#include <set>
#include "portaudio.h"

#define SAMPLE_RATE         (44100)
#define FRAMES_PER_BUFFER   (1024)

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

//===========================================================================
// GENERATOR
void updatePhase(float frequency, float& phase) {
    phase += (2 * M_PI) * frequency / SAMPLE_RATE;
    if (phase >= (2 * M_PI)) {
        phase -= (2 * M_PI);
    }
}

// Wave function definitions
float sineWave(float frequency, float& phase) {
    float value = sinf(phase);
    updatePhase(frequency, phase);
    return value;
}
float sawWave(float frequency, float& phase) {
    float value = 2.0 * (phase / (2 * M_PI)) - 1.0;
    updatePhase(frequency, phase);
    return value;
}
float squareWave(float frequency, float& phase) {
    float value;
    if (phase < M_PI)     value = 1.0f;
    else                value = -1.0f;
    updatePhase(frequency, phase);
    return value;
}
float triangleWave(float frequency, float& phase) {
    float value;
    if (phase < M_PI)     value = -1.0f + (2 / M_PI) * phase;
    else                value = 3.0f - (2 / M_PI) * phase;
    updatePhase(frequency, phase);
    return value;
}
float noise(float frequency, float& phase) {
    float value = ((rand() % 3) - 1);
    updatePhase(frequency, phase);
    return value;
}

float generator(float& sampleValue, std::string& waveform, std::set<float>& activeFrequencies, std::unordered_map<float, float>& phases) {
    for (float freq : activeFrequencies) {
        float phaseIncrement = (2 * M_PI) * freq / SAMPLE_RATE;
        if (waveform == "sine") {
            sampleValue += sineWave(freq, phases[freq]);
        }
        else if (waveform == "saw") {
            sampleValue += sawWave(freq, phases[freq]);
        }
        else if (waveform == "square") {
            sampleValue += squareWave(freq, phases[freq]);
        }
        else if (waveform == "triangle") {
            sampleValue += triangleWave(freq, phases[freq]);
        }
        else if (waveform == "noise") {
            sampleValue += noise(freq, phases[freq]);
        }

        phases[freq] += phaseIncrement;
        if (phases[freq] >= (2 * M_PI)) {
            phases[freq] -= (2 * M_PI);
        }
    }
    sampleValue /= activeFrequencies.size() > 0 ? activeFrequencies.size() : 1;
    return sampleValue;
}

//======================================================================================================
// FILTER

// Lowpass filter coefficients
float a0, a1, a2, b0, b1, b2 = 0.0f;
float x_1, x_2, y_1, y_2 = 0.0f;

void setLowPass(float cutoff, float Q) {  // function to set the cutoff and resonance (Q)
    float w0 = 2.0f * M_PI * cutoff / SAMPLE_RATE;
    float alpha = sin(w0) / (2.0f * Q);
    float cosw0 = cos(w0);

    b0 = (1.0f - cosw0) / 2.0f;
    b1 = 1.0f - cosw0;
    b2 = b0;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cosw0;
    a2 = 1.0f - alpha;

    b0 /= a0;   // normalize
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

float filter(float in) {
    float out = b0 * in + b1 * x_1 + b2 * x_2 - a1 * y_1 - a2 * y_2;

    x_2 = x_1;
    x_1 = in;

    y_2 = y_1;
    y_1 = out;

    return out;
}
//============================================================================
int main() {
    PaStreamParameters outputParameters;
    PaStream* stream;
    PaError err;
    float buffer[FRAMES_PER_BUFFER][2]; /* stereo output buffer */

    srand(2);
    std::set<float> activeFrequencies;           // core
    std::unordered_map<float, float> phases;    //  core
    std::string waveform = "sine";

    err = Pa_Initialize();
    if (err != paNoError)
        goto error;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice)
    {
        fprintf(stderr, "Error: No default output device.\n");
        goto error;
    }
    outputParameters.channelCount = 2;       /* stereo output */
    outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency; // Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &stream,
        NULL, /* no input */
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paClipOff,      /* we won't output out of range samples so don't bother clipping them */
        NULL,           /* no callback, use blocking API */
        NULL);          /* no callback, so no callback userData */
    if (err != paNoError)
        goto error;

        err = Pa_StartStream(stream);
        if (err != paNoError)
            goto error;

//===========================================================================


        // Controls:
        // Generator
        activeFrequencies.insert(110.0f);       // A major chord
        activeFrequencies.insert(220.0f);
        activeFrequencies.insert(277.18f);
        activeFrequencies.insert(329.63f);
        activeFrequencies.insert(440.0f);

        waveform = "saw";                       // options are "sine", "saw", "square", "triangle", and "noise"

        // Filter
        setLowPass(1000.0f, 2.0f);            // cutoff in hz


//==================================================================
        while (true) {

                 for (int j = 0; j < FRAMES_PER_BUFFER; j++) {
                        float sampleValue = 0.0f;

                        // WHAT MATTERS:
                        sampleValue = generator(sampleValue, waveform, activeFrequencies, phases);
                        sampleValue = filter(sampleValue);

                        // to tidy stuff up
                        sampleValue *= 0.5;
                        if (sampleValue > 1.0f) sampleValue = 1.0f;
                        if (sampleValue < -1.0f) sampleValue = -1.0f;

                        buffer[j][0] = sampleValue; // writing to the buffers for left & right speakers
                        buffer[j][1] = sampleValue;
                 }
                 err = Pa_WriteStream(stream, buffer, FRAMES_PER_BUFFER);
                 //if (err != paNoError)
                 //    goto error;

        }
//===============================================================

        err = Pa_StopStream(stream);
        if (err != paNoError)
            goto error;

    err = Pa_CloseStream(stream);
    if (err != paNoError)
        goto error;

    Pa_Terminate();
    printf("Test finished.\n");

    return err;

error:
    fprintf(stderr, "An error occurred while using the portaudio stream\n");
    fprintf(stderr, "Error number: %d\n", err);
    fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));

    if (err == paUnanticipatedHostError)
    {
        const PaHostErrorInfo* hostErrorInfo = Pa_GetLastHostErrorInfo();
        fprintf(stderr, "Host API error = #%ld, hostApiType = %d\n", hostErrorInfo->errorCode, hostErrorInfo->hostApiType);
        fprintf(stderr, "Host API error = %s\n", hostErrorInfo->errorText);
    }
    Pa_Terminate();
    return err;
}
