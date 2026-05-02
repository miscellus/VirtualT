/* sound.c */

/* $Id: sound.c,v 1.10 2013/02/25 00:52:28 kpettit1 Exp $ */

/*
 * Copyright 2005 Ken Pettit
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <unistd.h>
#endif

#include <SDL3/SDL.h>

#include "VirtualT.h"
#include "sound.h"
#include "m100emu.h"

#define             BLOCK_SIZE              1024
#define             SAMPLING_RATE           22050
#define             NUM_CHANNELS            2
#define             MAX_TONE_CYCLES         5000000
#define             TONE_TRANSITION_SAMPLES 16
#define             DECAY_MAX_LEVEL         4.5

#define             TONE_STOPPED            0
#define             TONE_START              1
#define             TONE_PLAYING            2
#define             TONE_STOP               3

/* Extern data */
extern  UINT64      cycles;
extern  int         cycle_delta;

/*
 * module static data
 */
static int          gReqFreq[16];
static int          gReqIn = 0;
static int          gReqOut = 0;
static int          gReqLastFreq = 0;

unsigned short      gpOneHertz[SAMPLING_RATE];
static int          gPlayTone = TONE_STOPPED;
static int          gToneFreq = 0;
static int          gExit = 0;
static int          gBeepOn = 0;
static int          gOneHzPtr = 0;
static double       gToneDivisor = 1.0;
static double       gDecayLevel = DECAY_MAX_LEVEL;
static double       gDecayStep = 0.008;
static int          gLastToneFreq = 0;
static int          gStepCount = 0;
static  UINT64      spkr_cycle = 0;
static  UINT64      gPlayCycle = 0;
int                 sound_enable = 1;

/* SDL3 Audio specifics */
static SDL_AudioStream *gAudioStream = NULL;
static SDL_Mutex       *gReqMutex = NULL;

/*
========================================================================
sound_generate_audio:	Creates and plays tones, smoothing frequencies
                        and dynamically applying volume envelopes.
========================================================================
*/
static void sound_generate_audio(Uint8 *stream_buf, int len)
{
    int i;
    int samples = len >> 1; // Number of 16-bit samples
    short *out = (short *)stream_buf;
    double toneStep = 0.0;

    /* Process pending frequency requests safely */
    SDL_LockMutex(gReqMutex);
    while (gReqIn != gReqOut)
    {
        int newFreq = gReqFreq[gReqOut++];
        if (gReqOut >= 16)
            gReqOut = 0;

        if (newFreq == 0)
        {
            /* Stop the currently playing tone */
            if (gPlayTone == TONE_PLAYING)
                gPlayTone = TONE_STOP;
        }
        else if ((newFreq > 0) && (gPlayTone != TONE_PLAYING))
        {
            /* Start new tone */
            gToneFreq = newFreq;
            gPlayTone = TONE_START;
            gPlayCycle = cycles;
        }
        else
        {
            /* Change the tone frequency */
            gToneFreq = newFreq;
        }
    }
    SDL_UnlockMutex(gReqMutex);

    /* Test for runaway tones */
    if (gPlayTone != TONE_STOPPED)
    {
        if (cycles - gPlayCycle > MAX_TONE_CYCLES)
            gPlayTone = TONE_STOP;
    }

    /* Test if the "beep" command is active */
    if (gBeepOn)
    {
        UINT64 dc = cycles - spkr_cycle;
        /* Wait for the "beep" time to expire and turn it off */
        if (dc > 15000)
        {
            gPlayTone = TONE_STOP;
            gBeepOn = 0;
            gReqLastFreq = 0;
        }
    }

    /* Loop for all samples and create the buffer */
    for (i = 0; i < samples; i += 2)
    {
        if (gPlayTone == TONE_PLAYING)
        {
            out[i] = (short)gpOneHertz[gOneHzPtr];
            out[i + 1] = out[i];
        }
        else if (gPlayTone == TONE_START || gPlayTone == TONE_STOP)
        {
            unsigned short val = (unsigned short) (((double) (signed short) gpOneHertz[gOneHzPtr]) * exp(-gDecayLevel));
            out[i] = (short)val;
            out[i + 1] = (short)val;

            if (gPlayTone == TONE_START)
            {
                gDecayLevel -= gDecayStep;
                if (gDecayLevel <= 0.0)
                {
                    gDecayLevel = 0.0;
                    gPlayTone = TONE_PLAYING;
                }
            }
            else /* TONE_STOP */
            {
                gDecayLevel += gDecayStep * 1.75;
                if (gDecayLevel >= DECAY_MAX_LEVEL)
                {
                    gDecayLevel = DECAY_MAX_LEVEL;
                    gPlayTone = TONE_STOPPED;
                }
            }
        }
        else /* TONE_STOPPED */
        {
            out[i] = 0;
            out[i + 1] = 0;
        }

        /* Update pointer in the 1 Hz waveform based on frequency */
        if (gPlayTone != TONE_STOPPED)
        {
            if (gToneFreq != gLastToneFreq && gLastToneFreq != 0)
            {
                if (++gStepCount >= TONE_TRANSITION_SAMPLES)
                {
                    if (gToneFreq != 0)
                        gLastToneFreq = gToneFreq;
                    gOneHzPtr += gToneFreq;
                }
                else
                {
                    toneStep = (double)(gToneFreq - gLastToneFreq) / (double)TONE_TRANSITION_SAMPLES;
                    gOneHzPtr += gLastToneFreq + (int)(toneStep * (double)gStepCount);
                }
            }
            else
            {
                gOneHzPtr += gToneFreq;
                gLastToneFreq = gToneFreq;
                gStepCount = 0;
            }

            if (gOneHzPtr >= SAMPLING_RATE)
            {
                gOneHzPtr -= SAMPLING_RATE;
            }
        }
    }
}

/*
========================================================================
audio_callback:	SDL3 Audio Stream callback
========================================================================
*/
static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
    if (additional_amount <= 0) return;

    /* Allocate buffer for requested output amount */
    Uint8 *buffer = (Uint8 *)SDL_malloc(additional_amount);
    if (!buffer) return;

    /* Fill buffer */
    sound_generate_audio(buffer, additional_amount);

    /* Push the audio and cleanup */
    SDL_PutAudioStreamData(stream, buffer, additional_amount);
    SDL_free(buffer);
}

void sound_reset_output(void)
{
    if (gAudioStream)
    {
        /* Simply flush any pending SDL3 Audio Buffers */
        SDL_ClearAudioStream(gAudioStream);
    }
    gOneHzPtr = 0;
}

/*
==================================================================
init_sound:	This routine initializes the sound output device(s)
			for tone generations.
==================================================================
*/
void init_sound(void)
{
    int     x;
    double  w;

    /* Create sin table for 1Hz */
    w = 2.0 * 3.1415926536 / (double) SAMPLING_RATE;
    for (x = 0; x < SAMPLING_RATE; x++)
    {
        gpOneHertz[x] = (unsigned short) (sin(w * (double) x) * 32767.0);
    }

    /* Initialize the emulation cycles count for speaker frequency calcs */
    spkr_cycle = cycles;
    gExit = 0;

    gReqMutex = SDL_CreateMutex();

    /* Prepare SDL Audio Spec for 16-Bit Stereo Audio */
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = SAMPLING_RATE;
    spec.format = SDL_AUDIO_S16;
    spec.channels = NUM_CHANNELS;

    /* Open stream and resume device */
    gAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, NULL);
    if (gAudioStream)
    {
        SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(gAudioStream));
    }
}

/*
==================================================================
deinit_sound:	This routine deinitializes the sound output device(s)
				for tone generations.
==================================================================
*/
void deinit_sound(void)
{
    gExit = 1;

    /* Close the output device and Mutex */
    if (gAudioStream)
    {
        SDL_DestroyAudioStream(gAudioStream);
        gAudioStream = NULL;
    }

    if (gReqMutex)
    {
        SDL_DestroyMutex(gReqMutex);
        gReqMutex = NULL;
    }
}


/*
==================================================================
start_tone:	This routine starts a tone of specified frequency
==================================================================
*/
void sound_start_tone(int freq)
{
    /* Validate sound is enabled */
    if (!sound_enable)
        return;

    if ((freq < SAMPLING_RATE / 2.0) && (freq != gReqLastFreq))
    {
        gReqLastFreq = freq;
        /* Issue request for new frequency generation safely */
        SDL_LockMutex(gReqMutex);
        gReqFreq[gReqIn++] = freq;
        if (gReqIn >= 16)
            gReqIn = 0;
        SDL_UnlockMutex(gReqMutex);
    }
}

/*
==================================================================
stop_tone:	This routine stops playing of tones
==================================================================
*/
void sound_stop_tone(void)
{
    /* Validate sound is enabled */
    if (!sound_enable)
        return;

    /* Issue a request for a frequency of zero */
    sound_start_tone(0);
}

/*
==================================================================
toggle_speaker:	This routine handles toggling of the I/O bit that
				is connected directly to the speaker.  The routine
				calculates the frequency of toggle and generates
				a beep during the toggle period.
==================================================================
*/
void sound_toggle_speaker(int bitVal)
{
    UINT64 delta;

    /* Validate sound is enabled */
    if (!sound_enable)
        return;

    /* Calculate delta between current cycle and last cycle */
    delta = cycles + cycle_delta - spkr_cycle;
    spkr_cycle = cycles + cycle_delta;

    /* Test if delta is within a valid range */
    if ((delta < 5000) && (delta != 0))
    {
        /* Indicate the "Beep" is on and set the frequency */
        gBeepOn = 1;
        sound_start_tone((int) (2400000.0 / delta / 2));
    }
}

/*
==================================================================
This routine sets the tone control divisor
==================================================================
*/
void sound_set_tone_control(double tone)
{
    /* Don't allow tone divisor of zero */
    if (tone == 0.0)
        tone = 1.0;

    /* Set the tone divisor */
    gToneDivisor = tone;
    gDecayStep = tone / (double) (BLOCK_SIZE >> 3);
}

/*
==================================================================
This routine gets the tone control divisor
==================================================================
*/
double sound_get_tone_control(void)
{
    return gToneDivisor;
}