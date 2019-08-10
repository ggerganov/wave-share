/*! \file main.cpp
 *  \brief Send/Receive data through sound
 *  \author Georgi Gerganov
 */

#include "fftw3.h"
#include "reed-solomon/rs.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include <cmath>
#include <cstdio>
#include <array>
#include <string>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifdef __EMSCRIPTEN__
#include "build_timestamp.h"
#include "emscripten/emscripten.h"
#else
#include <thread>
#include <iostream>
#endif

#ifdef main
#undef main
#endif

static char *g_captureDeviceName = nullptr;
static int g_captureId = -1;
static int g_playbackId = -1;

static bool g_isInitialized = false;
static int g_totalBytesCaptured = 0;

static SDL_AudioDeviceID devid_in = 0;
static SDL_AudioDeviceID devid_out = 0;

struct DataRxTx;
static DataRxTx *g_data = nullptr;

namespace {
    //constexpr float IRAND_MAX = 1.0f/RAND_MAX;
    //inline float frand() { return ((float)(rand()%RAND_MAX)*IRAND_MAX); }

    constexpr double kBaseSampleRate = 48000.0;
    constexpr auto kMaxSamplesPerFrame = 1024;
    constexpr auto kMaxDataBits = 256;
    constexpr auto kMaxDataSize = 256;
    constexpr auto kMaxLength = 140;
    constexpr auto kMaxSpectrumHistory = 4;
    constexpr auto kMaxRecordedFrames = 64*10;
    constexpr auto kDefaultFixedLength = 82;

    enum TxMode {
        FixedLength = 0,
        VariableLength,
    };

    using AmplitudeData   = std::array<float, kMaxSamplesPerFrame>;
    using AmplitudeData16 = std::array<int16_t, kMaxRecordedFrames*kMaxSamplesPerFrame>;
    using SpectrumData    = std::array<float, kMaxSamplesPerFrame>;
    using RecordedData    = std::array<float, kMaxRecordedFrames*kMaxSamplesPerFrame>;

    inline void addAmplitudeSmooth(const AmplitudeData & src, AmplitudeData & dst, float scalar, int startId, int finalId, int cycleMod, int nPerCycle) {
        int nTotal = nPerCycle*finalId;
        float frac = 0.15f;
        float ds = frac*nTotal;
        float ids = 1.0f/ds;
        int nBegin = frac*nTotal;
        int nEnd = (1.0f - frac)*nTotal;
        for (int i = startId; i < finalId; i++) {
            float k = cycleMod*finalId + i;
            if (k < nBegin) {
                dst[i] += scalar*src[i]*(k*ids);
            } else if (k > nEnd) {
                dst[i] += scalar*src[i]*(((float)(nTotal) - k)*ids);
            } else {
                dst[i] += scalar*src[i];
            }
        }
    }

    template <class T>
        float getTime_ms(const T & tStart, const T & tEnd) {
            return ((float)(std::chrono::duration_cast<std::chrono::microseconds>(tEnd - tStart).count()))/1000.0;
        }

    int getECCBytesForLength(int len) {
        return std::max(4, 2*(len/5));
    }
}

struct DataRxTx {
    DataRxTx(int aSampleRateOut, int aSampleRate, int aSamplesPerFrame, int aSampleSizeB, const char * text) {
        sampleSizeBytes = aSampleSizeB;
        sampleRate = aSampleRate;
        sampleRateOut = aSampleRateOut;
        samplesPerFrame = aSamplesPerFrame;

        init(strlen(text), text);
    }

    void init(int textLength, const char * stext) {
        if (textLength > ::kMaxLength) {
            printf("Truncating data from %d to 140 bytes\n", textLength);
            textLength = ::kMaxLength;
        }

        const uint8_t * text = reinterpret_cast<const uint8_t *>(stext);
        frameId = 0;
        nIterations = 0;
        hasData = false;

        isamplesPerFrame = 1.0f/samplesPerFrame;
        sendVolume = ((double)(paramVolume))/100.0f;
        hzPerFrame = sampleRate/samplesPerFrame;
        ihzPerFrame = 1.0/hzPerFrame;
        framesPerTx = paramFramesPerTx;

        nDataBitsPerTx = paramBytesPerTx*8;
        nECCBytesPerTx = (txMode == ::TxMode::FixedLength) ? paramECCBytesPerTx : getECCBytesForLength(textLength);

        framesToAnalyze = 0;
        framesLeftToAnalyze = 0;
        framesToRecord = 0;
        framesLeftToRecord = 0;
        nBitsInMarker = 16;
        nMarkerFrames = 16;
        nPostMarkerFrames = 0;
        sendDataLength = (txMode == ::TxMode::FixedLength) ? ::kDefaultFixedLength : textLength + 3;

        d0 = paramFreqDelta/2;
        freqDelta_hz = hzPerFrame*paramFreqDelta;
        freqStart_hz = hzPerFrame*paramFreqStart;
        if (paramFreqDelta == 1) {
            d0 = 1;
            freqDelta_hz *= 2;
        }

        outputBlock.fill(0);
        encodedData.fill(0);

        for (int k = 0; k < (int) phaseOffsets.size(); ++k) {
            phaseOffsets[k] = (M_PI*k)/(nDataBitsPerTx);
        }
#ifdef __EMSCRIPTEN__
        std::random_shuffle(phaseOffsets.begin(), phaseOffsets.end());
#endif

        for (int k = 0; k < (int) dataBits.size(); ++k) {
            double freq = freqStart_hz + freqDelta_hz*k;
            dataFreqs_hz[k] = freq;

            double phaseOffset = phaseOffsets[k];
            double curHzPerFrame = sampleRateOut/samplesPerFrame;
            double curIHzPerFrame = 1.0/curHzPerFrame;
            for (int i = 0; i < samplesPerFrame; i++) {
                double curi = i;
                bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*(freq*curIHzPerFrame) + phaseOffset);
            }
            for (int i = 0; i < samplesPerFrame; i++) {
                double curi = i;
                bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*((freq + hzPerFrame*d0)*curIHzPerFrame) + phaseOffset);
            }
        }

        if (rsData) delete rsData;
        if (rsLength) delete rsLength;

        if (txMode == ::TxMode::FixedLength) {
            rsData = new RS::ReedSolomon(kDefaultFixedLength, nECCBytesPerTx);
        } else {
            rsData = new RS::ReedSolomon(textLength, nECCBytesPerTx);
            rsLength = new RS::ReedSolomon(1, 2);
        }

        if (textLength > 0) {
            static std::array<char, ::kMaxDataSize> theData;
            theData.fill(0);

            if (txMode == ::TxMode::FixedLength) {
                for (int i = 0; i < textLength; ++i) theData[i] = text[i];
                rsData->Encode(theData.data(), encodedData.data());
            } else {
                theData[0] = textLength;
                for (int i = 0; i < textLength; ++i) theData[i + 1] = text[i];
                rsData->Encode(theData.data() + 1, encodedData.data() + 3);
                rsLength->Encode(theData.data(), encodedData.data());
            }

            hasData = true;
        }

        // Rx
        receivingData = false;
        analyzingData = false;

        sampleAmplitude.fill(0);

        sampleSpectrum.fill(0);
        for (auto & s : sampleAmplitudeHistory) {
            s.fill(0);
        }

        rxData.fill(0);

        if (fftPlan) fftwf_destroy_plan(fftPlan);
        if (fftIn) fftwf_free(fftIn);
        if (fftOut) fftwf_free(fftOut);

        fftIn = (float*) fftwf_malloc(sizeof(float)*samplesPerFrame);
        fftOut = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex)*samplesPerFrame);
        fftPlan = fftwf_plan_dft_r2c_1d(1*samplesPerFrame, fftIn, fftOut, FFTW_ESTIMATE);

        for (int i = 0; i < samplesPerFrame; ++i) {
            fftOut[i][0] = 0.0f;
            fftOut[i][1] = 0.0f;
        }
    }

    void send() {
        int samplesPerFrameOut = (sampleRateOut/sampleRate)*samplesPerFrame;
        if (sampleRateOut != sampleRate) {
            printf("Resampling from %d Hz to %d Hz\n", (int) sampleRate, (int) sampleRateOut);
        }

        while(hasData) {
            int nBytesPerTx = nDataBitsPerTx/8;
            std::fill(outputBlock.begin(), outputBlock.end(), 0.0f);
            std::uint16_t nFreq = 0;

            if (sampleRateOut != sampleRate) {
                for (int k = 0; k < nDataBitsPerTx; ++k) {
                    double freq = freqStart_hz + freqDelta_hz*k;

                    double phaseOffset = phaseOffsets[k];
                    double curHzPerFrame = sampleRateOut/samplesPerFrame;
                    double curIHzPerFrame = 1.0/curHzPerFrame;
                    for (int i = 0; i < samplesPerFrameOut; i++) {
                        double curi = (i + frameId*samplesPerFrameOut);
                        bit1Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*(freq*curIHzPerFrame) + phaseOffset);
                    }
                    for (int i = 0; i < samplesPerFrameOut; i++) {
                        double curi = (i + frameId*samplesPerFrameOut);
                        bit0Amplitude[k][i] = std::sin((2.0*M_PI)*(curi*isamplesPerFrame)*((freq + hzPerFrame*d0)*curIHzPerFrame) + phaseOffset);
                    }
                }
            }

            if (frameId < nMarkerFrames) {
                nFreq = nBitsInMarker;

                for (int i = 0; i < nBitsInMarker; ++i) {
                    if (i%2 == 0) {
                        ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId, nMarkerFrames);
                    } else {
                        ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId, nMarkerFrames);
                    }
                }
            } else if (frameId < nMarkerFrames + nPostMarkerFrames) {
                nFreq = nBitsInMarker;

                for (int i = 0; i < nBitsInMarker; ++i) {
                    if (i%2 == 0) {
                        ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId - nMarkerFrames, nPostMarkerFrames);
                    } else {
                        ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, frameId - nMarkerFrames, nPostMarkerFrames);
                    }
                }
            } else if (frameId <
                       (nMarkerFrames + nPostMarkerFrames) +
                       ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*framesPerTx) {
                int dataOffset = frameId - nMarkerFrames - nPostMarkerFrames;
                int cycleModMain = dataOffset%framesPerTx;
                dataOffset /= framesPerTx;
                dataOffset *= nBytesPerTx;

                dataBits.fill(0);

                if (paramFreqDelta > 1) {
                    for (int j = 0; j < nBytesPerTx; ++j) {
                        for (int i = 0; i < 8; ++i) {
                            dataBits[j*8 + i] = encodedData[dataOffset + j] & (1 << i);
                        }
                    }

                    for (int k = 0; k < nDataBitsPerTx; ++k) {
                        ++nFreq;
                        if (dataBits[k] == false) {
                            ::addAmplitudeSmooth(bit0Amplitude[k], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, framesPerTx);
                            continue;
                        }
                        ::addAmplitudeSmooth(bit1Amplitude[k], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, framesPerTx);
                    }
                } else {
                    for (int j = 0; j < nBytesPerTx; ++j) {
                        {
                            uint8_t d = encodedData[dataOffset + j] & 15;
                            dataBits[(2*j + 0)*16 + d] = 1;
                        }
                        {
                            uint8_t d = encodedData[dataOffset + j] & 240;
                            dataBits[(2*j + 1)*16 + (d >> 4)] = 1;
                        }
                    }

                    for (int k = 0; k < 2*nBytesPerTx*16; ++k) {
                        if (dataBits[k] == 0) continue;

                        ++nFreq;
                        if (k%2) {
                            ::addAmplitudeSmooth(bit0Amplitude[k/2], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, framesPerTx);
                        } else {
                            ::addAmplitudeSmooth(bit1Amplitude[k/2], outputBlock, sendVolume, 0, samplesPerFrameOut, cycleModMain, framesPerTx);
                        }
                    }
                }
            } else if (txMode == ::TxMode::VariableLength && frameId <
                       (nMarkerFrames + nPostMarkerFrames) +
                       ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*framesPerTx +
                       (nMarkerFrames)) {
                nFreq = nBitsInMarker;

                int fId = frameId - ((nMarkerFrames + nPostMarkerFrames) + ((sendDataLength + nECCBytesPerTx)/nBytesPerTx + 2)*framesPerTx);
                for (int i = 0; i < nBitsInMarker; ++i) {
                    if (i%2 == 0) {
                        ::addAmplitudeSmooth(bit0Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, fId, nMarkerFrames);
                    } else {
                        ::addAmplitudeSmooth(bit1Amplitude[i], outputBlock, sendVolume, 0, samplesPerFrameOut, fId, nMarkerFrames);
                    }
                }
            } else {
                textToSend = "";
                hasData = false;
            }

            if (nFreq == 0) nFreq = 1;
            float scale = 1.0f/nFreq;
            for (int i = 0; i < samplesPerFrameOut; ++i) {
                outputBlock[i] *= scale;
            }

            for (int i = 0; i < samplesPerFrameOut; ++i) {
                outputBlock16[frameId*samplesPerFrameOut + i] = std::round(32000.0*outputBlock[i]);
            }
            ++frameId;
        }
        SDL_QueueAudio(devid_out, outputBlock16.data(), 2*frameId*samplesPerFrameOut);
    }

    void receive() {
        static int nCalls = 0;
        static float tSum_ms = 0.0f;
        auto tCallStart = std::chrono::high_resolution_clock::now();

        if (needUpdate) {
            init(0, "");
            needUpdate = false;
        }

        while (hasData == false) {
            // read capture data
            int nBytesRecorded = SDL_DequeueAudio(devid_in, sampleAmplitude.data(), samplesPerFrame*sampleSizeBytes);
            if (nBytesRecorded != 0) {
                {
                    sampleAmplitudeHistory[historyId] = sampleAmplitude;

                    if (++historyId >= ::kMaxSpectrumHistory) {
                        historyId = 0;
                    }

                    if (historyId == 0 && (receivingData == false || (receivingData && txMode == ::TxMode::VariableLength))) {
                        std::fill(sampleAmplitudeAverage.begin(), sampleAmplitudeAverage.end(), 0.0f);
                        for (auto & s : sampleAmplitudeHistory) {
                            for (int i = 0; i < samplesPerFrame; ++i) {
                                sampleAmplitudeAverage[i] += s[i];
                            }
                        }
                        float norm = 1.0f/::kMaxSpectrumHistory;
                        for (int i = 0; i < samplesPerFrame; ++i) {
                            sampleAmplitudeAverage[i] *= norm;
                        }

                        // calculate spectrum
                        std::copy(sampleAmplitudeAverage.begin(), sampleAmplitudeAverage.begin() + samplesPerFrame, fftIn);

                        fftwf_execute(fftPlan);

                        double fsum = 0.0;
                        for (int i = 0; i < samplesPerFrame; ++i) {
                            sampleSpectrum[i] = (fftOut[i][0]*fftOut[i][0] + fftOut[i][1]*fftOut[i][1]);
                            fsum += sampleSpectrum[i];
                        }
                        for (int i = 1; i < samplesPerFrame/2; ++i) {
                            sampleSpectrum[i] += sampleSpectrum[samplesPerFrame - i];
                        }

                        if (fsum < 1e-10) {
                            g_totalBytesCaptured = 0;
                        } else {
                            g_totalBytesCaptured += nBytesRecorded;
                        }
                    }

                    if (framesLeftToRecord > 0) {
                        std::copy(sampleAmplitude.begin(),
                                  sampleAmplitude.begin() + samplesPerFrame,
                                  recordedAmplitude.data() + (framesToRecord - framesLeftToRecord)*samplesPerFrame);

                        if (--framesLeftToRecord <= 0) {
                            std::fill(sampleSpectrum.begin(), sampleSpectrum.end(), 0.0f);
                            analyzingData = true;
                        }
                    }
                }

                if (analyzingData) {
                    int nBytesPerTx = nDataBitsPerTx/8;
                    int stepsPerFrame = 16;
                    int step = samplesPerFrame/stepsPerFrame;

                    int offsetStart = 0;

                    framesToAnalyze = nMarkerFrames*stepsPerFrame;
                    framesLeftToAnalyze = framesToAnalyze;

                    bool isValid = false;
                    //for (int ii = nMarkerFrames*stepsPerFrame/2; ii < (nMarkerFrames + nPostMarkerFrames)*stepsPerFrame; ++ii) {
                    for (int ii = nMarkerFrames*stepsPerFrame - 1; ii >= nMarkerFrames*stepsPerFrame/2; --ii) {
                        offsetStart = ii;
                        bool knownLength = txMode == ::TxMode::FixedLength;
                        int encodedOffset = (txMode == ::TxMode::FixedLength) ? 0 : 3;

                        for (int itx = 0; itx < 1024; ++itx) {
                            int offsetTx = offsetStart + itx*framesPerTx*stepsPerFrame;
                            if (offsetTx >= recvDuration_frames*stepsPerFrame) {
                                break;
                            }

                            std::copy(
                                recordedAmplitude.begin() + offsetTx*step,
                                recordedAmplitude.begin() + offsetTx*step + samplesPerFrame, fftIn);

                            for (int k = 1; k < framesPerTx-1; ++k) {
                                for (int i = 0; i < samplesPerFrame; ++i) {
                                    fftIn[i] += recordedAmplitude[(offsetTx + k*stepsPerFrame)*step + i];
                                }
                            }

                            fftwf_execute(fftPlan);

                            for (int i = 0; i < samplesPerFrame; ++i) {
                                sampleSpectrum[i] = (fftOut[i][0]*fftOut[i][0] + fftOut[i][1]*fftOut[i][1]);
                            }
                            for (int i = 1; i < samplesPerFrame/2; ++i) {
                                sampleSpectrum[i] += sampleSpectrum[samplesPerFrame - i];
                            }

                            uint8_t curByte = 0;
                            if (paramFreqDelta > 1) {
                                for (int i = 0; i < nDataBitsPerTx; ++i) {
                                    int k = i%8;
                                    int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);
                                    if (sampleSpectrum[bin] > 1*sampleSpectrum[bin + d0]) {
                                        curByte += 1 << k;
                                    } else if (sampleSpectrum[bin + d0] > 1*sampleSpectrum[bin]) {
                                    } else {
                                    }
                                    if (k == 7) {
                                        encodedData[itx*nBytesPerTx + i/8] = curByte;
                                        curByte = 0;
                                    }
                                }
                            } else {
                                for (int i = 0; i < 2*nBytesPerTx; ++i) {
                                    int bin = std::round(dataFreqs_hz[0]*ihzPerFrame) + i*16;

                                    int kmax = 0;
                                    double amax = 0.0;
                                    for (int k = 0; k < 16; ++k) {
                                        if (sampleSpectrum[bin + k] > amax) {
                                            kmax = k;
                                            amax = sampleSpectrum[bin + k];
                                        }
                                    }

                                    if (i%2) {
                                        curByte += (kmax << 4);
                                        encodedData[itx*nBytesPerTx + i/2] = curByte;
                                        curByte = 0;
                                    } else {
                                        curByte = kmax;
                                    }
                                }
                            }

                            if (txMode == ::TxMode::VariableLength) {
                                if (itx*nBytesPerTx > 3 && knownLength == false) {
                                    if ((rsLength->Decode(encodedData.data(), rxData.data()) == 0) && (rxData[0] <= 140)) {
                                        knownLength = true;
                                    } else {
                                        break;
                                    }
                                }
                            }
                        }

                        if (txMode == ::TxMode::VariableLength && knownLength) {
                            if (rsData) delete rsData;
                            rsData = new RS::ReedSolomon(rxData[0], ::getECCBytesForLength(rxData[0]));
                        }

                        if (knownLength) {
                            int decodedLength = rxData[0];
                            if (rsData->Decode(encodedData.data() + encodedOffset, rxData.data()) == 0) {
                                printf("Decoded length = %d\n", decodedLength);
                                if (txMode == ::TxMode::FixedLength && rxData[0] == 'A') {
                                    printf("[ANSWER] Received sound data successfully!\n");
                                } else if (txMode == ::TxMode::FixedLength && rxData[0] == 'O') {
                                    printf("[OFFER]  Received sound data successfully!\n");
                                } else {
                                    std::string s((char *) rxData.data(), decodedLength);
                                    printf("Received sound data successfully: '%s'\n", s.c_str());
                                }
                                framesToRecord = 0;
                                isValid = true;
                            }
                        }

                        if (isValid) {
                            break;
                        }
                        --framesLeftToAnalyze;
                    }

                    if (isValid == false) {
                        printf("Failed to capture sound data. Please try again\n");
                        framesToRecord = -1;
                    }

                    receivingData = false;
                    analyzingData = false;

                    std::fill(sampleSpectrum.begin(), sampleSpectrum.end(), 0.0f);

                    framesToAnalyze = 0;
                    framesLeftToAnalyze = 0;
                }

                // check if receiving data
                if (receivingData == false) {
                    bool isReceiving = true;

                    for (int i = 0; i < nBitsInMarker; ++i) {
                        int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);

                        if (i%2 == 0) {
                            if (sampleSpectrum[bin] <= 3.0f*sampleSpectrum[bin + d0]) isReceiving = false;
                        } else {
                            if (sampleSpectrum[bin] >= 3.0f*sampleSpectrum[bin + d0]) isReceiving = false;
                        }
                    }

                    if (isReceiving) {
                        std::time_t timestamp = std::time(nullptr);
                        printf("%sReceiving sound data ...\n", std::asctime(std::localtime(&timestamp)));
                        rxData.fill(0);
                        receivingData = true;
                        if (txMode == ::TxMode::FixedLength) {
                            recvDuration_frames = nMarkerFrames + nPostMarkerFrames + framesPerTx*((::kDefaultFixedLength + paramECCBytesPerTx)/paramBytesPerTx + 1);
                        } else {
                            recvDuration_frames = nMarkerFrames + nPostMarkerFrames + framesPerTx*((::kMaxLength + ::getECCBytesForLength(::kMaxLength))/paramBytesPerTx + 1);
                        }
                        framesToRecord = recvDuration_frames;
                        framesLeftToRecord = recvDuration_frames;
                    }
                } else if (txMode == ::TxMode::VariableLength) {
                    bool isEnded = true;

                    for (int i = 0; i < nBitsInMarker; ++i) {
                        int bin = std::round(dataFreqs_hz[i]*ihzPerFrame);

                        if (i%2 == 0) {
                            if (sampleSpectrum[bin] >= 3.0f*sampleSpectrum[bin + d0]) isEnded = false;
                        } else {
                            if (sampleSpectrum[bin] <= 3.0f*sampleSpectrum[bin + d0]) isEnded = false;
                        }
                    }

                    if (isEnded && framesToRecord > 1) {
                        std::time_t timestamp = std::time(nullptr);
                        printf("%sReceived end marker\n", std::asctime(std::localtime(&timestamp)));
                        recvDuration_frames -= framesLeftToRecord - 1;
                        framesLeftToRecord = 1;
                    }
                }
            } else {
                break;
            }

            ++nIterations;
        }

        auto tCallEnd = std::chrono::high_resolution_clock::now();
        tSum_ms += getTime_ms(tCallStart, tCallEnd);
        if (++nCalls == 10) {
            averageRxTime_ms = tSum_ms/nCalls;
            tSum_ms = 0.0f;
            nCalls = 0;
        }

        if ((int) SDL_GetQueuedAudioSize(devid_in) > 32*sampleSizeBytes*samplesPerFrame) {
            printf("nIter = %d, Queue size: %d\n", nIterations, SDL_GetQueuedAudioSize(devid_in));
            SDL_ClearQueuedAudio(devid_in);
        }
    }

    int nIterations;
    bool needUpdate = false;

    int paramFreqDelta = 6;
    int paramFreqStart = 40;
    int paramFramesPerTx = 6;
    int paramBytesPerTx = 2;
    int paramECCBytesPerTx = 32;
    int paramVolume = 10;

    // Rx
    bool receivingData;
    bool analyzingData;

    fftwf_plan fftPlan = 0;
    float *fftIn;
    fftwf_complex *fftOut = 0;

    ::AmplitudeData sampleAmplitude;
    ::SpectrumData sampleSpectrum;

    std::array<std::uint8_t, ::kMaxDataSize> rxData;
    std::array<std::uint8_t, ::kMaxDataSize> encodedData;

    int historyId = 0;
    ::AmplitudeData sampleAmplitudeAverage;
    std::array<::AmplitudeData, ::kMaxSpectrumHistory> sampleAmplitudeHistory;

    ::RecordedData recordedAmplitude;

    // Tx
    bool hasData;
    int sampleSizeBytes;
    float sampleRate;
    float sampleRateOut;
    int samplesPerFrame;
    float isamplesPerFrame;

    ::AmplitudeData outputBlock;
    ::AmplitudeData16 outputBlock16;

    std::array<::AmplitudeData, ::kMaxDataBits> bit1Amplitude;
    std::array<::AmplitudeData, ::kMaxDataBits> bit0Amplitude;

    float sendVolume;
    float hzPerFrame;
    float ihzPerFrame;

    int d0 = 1;
    float freqStart_hz;
    float freqDelta_hz;

    int frameId;
    int nRampFrames;
    int nRampFramesBegin;
    int nRampFramesEnd;
    int nRampFramesBlend;
    int dataId;
    int framesPerTx;
    int framesToAnalyze;
    int framesLeftToAnalyze;
    int framesToRecord;
    int framesLeftToRecord;
    int nBitsInMarker;
    int nMarkerFrames;
    int nPostMarkerFrames;
    int recvDuration_frames;

    ::TxMode txMode = ::TxMode::FixedLength;

    std::array<bool, ::kMaxDataBits> dataBits;
    std::array<double, ::kMaxDataBits> phaseOffsets;
    std::array<double, ::kMaxDataBits> dataFreqs_hz;

    int nDataBitsPerTx;
    int nECCBytesPerTx;
    int sendDataLength;

    RS::ReedSolomon * rsData = nullptr;
    RS::ReedSolomon * rsLength = nullptr;

    float averageRxTime_ms = 0.0;

    std::string textToSend;
};

int init() {
    if (g_isInitialized) return 0;

    printf("Initializing ...\n");

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        return (1);
    }

    SDL_SetHintWithPriority(SDL_HINT_AUDIO_RESAMPLING_MODE, "medium", SDL_HINT_OVERRIDE);

    {
        int nDevices = SDL_GetNumAudioDevices(SDL_FALSE);
        printf("Found %d playback devices:\n", nDevices);
        for (int i = 0; i < nDevices; i++) {
            printf("    - Playback device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_FALSE));
        }
    }
    {
        int nDevices = SDL_GetNumAudioDevices(SDL_TRUE);
        printf("Found %d capture devices:\n", nDevices);
        for (int i = 0; i < nDevices; i++) {
            printf("    - Capture device #%d: '%s'\n", i, SDL_GetAudioDeviceName(i, SDL_TRUE));
        }
    }

    SDL_AudioSpec desiredSpec;
    SDL_zero(desiredSpec);

    desiredSpec.freq = ::kBaseSampleRate;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = 1;
    desiredSpec.samples = 16*1024;
    desiredSpec.callback = NULL;

    SDL_AudioSpec obtainedSpec;
    SDL_zero(obtainedSpec);

    if (g_playbackId >= 0) {
        printf("Attempt to open playback device %d : '%s' ...\n", g_playbackId, SDL_GetAudioDeviceName(g_playbackId, SDL_FALSE));
        devid_out = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(g_playbackId, SDL_FALSE), SDL_FALSE, &desiredSpec, &obtainedSpec, 0);
    } else {
        printf("Attempt to open default playback device ...\n");
        devid_out = SDL_OpenAudioDevice(NULL, SDL_FALSE, &desiredSpec, &obtainedSpec, 0);
    }

    if (!devid_out) {
        printf("Couldn't open an audio device for playback: %s!\n", SDL_GetError());
        devid_out = 0;
    } else {
        printf("Obtained spec for output device (SDL Id = %d):\n", devid_out);
        printf("    - Sample rate:       %d (required: %d)\n", obtainedSpec.freq, desiredSpec.freq);
        printf("    - Format:            %d (required: %d)\n", obtainedSpec.format, desiredSpec.format);
        printf("    - Channels:          %d (required: %d)\n", obtainedSpec.channels, desiredSpec.channels);
        printf("    - Samples per frame: %d (required: %d)\n", obtainedSpec.samples, desiredSpec.samples);

        if (obtainedSpec.format != desiredSpec.format ||
            obtainedSpec.channels != desiredSpec.channels ||
            obtainedSpec.samples != desiredSpec.samples) {
            SDL_CloseAudio();
            throw std::runtime_error("Failed to initialize desired SDL_OpenAudio!");
        }
    }

    SDL_AudioSpec captureSpec;
    captureSpec = obtainedSpec;
    captureSpec.freq = ::kBaseSampleRate;
    captureSpec.format = AUDIO_F32SYS;
    captureSpec.samples = 1024;

    if (g_playbackId >= 0) {
        printf("Attempt to open capture device %d : '%s' ...\n", g_captureId, SDL_GetAudioDeviceName(g_captureId, SDL_FALSE));
        devid_in = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(g_captureId, SDL_TRUE), SDL_TRUE, &captureSpec, &captureSpec, 0);
    } else {
        printf("Attempt to open default capture device ...\n");
        devid_in = SDL_OpenAudioDevice(g_captureDeviceName, SDL_TRUE, &captureSpec, &captureSpec, 0);
    }
    if (!devid_in) {
        printf("Couldn't open an audio device for capture: %s!\n", SDL_GetError());
        devid_in = 0;
    } else {

        printf("Obtained spec for input device (SDL Id = %d):\n", devid_in);
        printf("    - Sample rate:       %d\n", captureSpec.freq);
        printf("    - Format:            %d (required: %d)\n", captureSpec.format, desiredSpec.format);
        printf("    - Channels:          %d (required: %d)\n", captureSpec.channels, desiredSpec.channels);
        printf("    - Samples per frame: %d\n", captureSpec.samples);
    }

    int sampleSizeBytes = 4;
    //switch (obtainedSpec.format) {
    //    case AUDIO_U8:
    //    case AUDIO_S8:
    //        sampleSizeBytes = 1;
    //        break;
    //    case AUDIO_U16SYS:
    //    case AUDIO_S16SYS:
    //        sampleSizeBytes = 2;
    //        break;
    //    case AUDIO_S32SYS:
    //    case AUDIO_F32SYS:
    //        sampleSizeBytes = 4;
    //        break;
    //}

    g_data = new DataRxTx(obtainedSpec.freq, ::kBaseSampleRate, captureSpec.samples, sampleSizeBytes, "");

    g_isInitialized = true;
    return 0;
}

// JS interface
extern "C" {
    int setText(int textLength, const char * text) {
        g_data->init(textLength, text);
        return 0;
    }

    int getText(char * text) {
        std::copy(g_data->rxData.begin(), g_data->rxData.end(), text);
        return 0;
    }

    int getSampleRate() { return g_data->sampleRate; }
    float getAverageRxTime_ms() { return g_data->averageRxTime_ms; }
    int getFramesToRecord() { return g_data->framesToRecord; }
    int getFramesLeftToRecord() { return g_data->framesLeftToRecord; }
    int getFramesToAnalyze() { return g_data->framesToAnalyze; }
    int getFramesLeftToAnalyze() { return g_data->framesLeftToAnalyze; }
    int hasDeviceOutput() { return devid_out; }
    int hasDeviceCapture() { return (g_totalBytesCaptured > 0) ? devid_in : 0; }
    int doInit() { return init(); }
    int setTxMode(int txMode) { g_data->txMode = (::TxMode)(txMode); return 0; }

    void setParameters(
        int paramFreqDelta,
        int paramFreqStart,
        int paramFramesPerTx,
        int paramBytesPerTx,
        int /*paramECCBytesPerTx*/,
        int paramVolume) {
        if (g_data == nullptr) return;

        g_data->paramFreqDelta = paramFreqDelta;
        g_data->paramFreqStart = paramFreqStart;
        g_data->paramFramesPerTx = paramFramesPerTx;
        g_data->paramBytesPerTx = paramBytesPerTx;
        g_data->paramVolume = paramVolume;

        g_data->needUpdate = true;
    }
}

// main loop
void update() {
    if (g_isInitialized == false) return;

    SDL_Event e;
    SDL_bool shouldTerminate = SDL_FALSE;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            shouldTerminate = SDL_TRUE;
        }
    }

    if (g_data->hasData == false) {
        SDL_PauseAudioDevice(devid_out, SDL_FALSE);

        static auto tLastNoData = std::chrono::high_resolution_clock::now();
        auto tNow = std::chrono::high_resolution_clock::now();

        if ((int) SDL_GetQueuedAudioSize(devid_out) < g_data->samplesPerFrame*g_data->sampleSizeBytes) {
            SDL_PauseAudioDevice(devid_in, SDL_FALSE);
            if (::getTime_ms(tLastNoData, tNow) > 500.0f) {
                g_data->receive();
            } else {
                SDL_ClearQueuedAudio(devid_in);
            }
        } else {
            tLastNoData = tNow;
            //SDL_ClearQueuedAudio(devid_in);
            //SDL_Delay(10);
        }
    } else {
        SDL_PauseAudioDevice(devid_out, SDL_TRUE);
        SDL_PauseAudioDevice(devid_in, SDL_TRUE);

        g_data->send();
    }

    if (shouldTerminate) {
        SDL_PauseAudioDevice(devid_in, 1);
        SDL_CloseAudioDevice(devid_in);
        SDL_PauseAudioDevice(devid_out, 1);
        SDL_CloseAudioDevice(devid_out);
        SDL_CloseAudio();
        SDL_Quit();
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        #endif
    }
}

static std::map<std::string, std::string> parseCmdArguments(int argc, char ** argv) {
    int last = argc;
    std::map<std::string, std::string> res;
    for (int i = 1; i < last; ++i) {
        if (argv[i][0] == '-') {
            if (strlen(argv[i]) > 1) {
                res[std::string(1, argv[i][1])] = strlen(argv[i]) > 2 ? argv[i] + 2 : "";
            }
        }
    }

    return res;
}

int main(int argc, char** argv) {
#ifdef __EMSCRIPTEN__
    printf("Build time: %s\n", BUILD_TIMESTAMP);
    printf("Press the Init button to start\n");

    g_captureDeviceName = argv[1];
#else
    printf("Usage: %s [-cN] [-pN] [-tN]\n", argv[0]);
    printf("    -cN - select capture device N\n");
    printf("    -pN - select playback device N\n");
    printf("    -tN - transmission protocol:\n");
    printf("          -t0 : Normal\n");
    printf("          -t1 : Fast (default)\n");
    printf("          -t2 : Fastest\n");
    printf("          -t3 : Ultrasonic\n");
    printf("\n");

    g_captureDeviceName = nullptr;

    auto argm = parseCmdArguments(argc, argv);
    g_captureId = argm["c"].empty() ? 0 : std::stoi(argm["c"]);
    g_playbackId = argm["p"].empty() ? 0 : std::stoi(argm["p"]);
    int txProtocol = argm["t"].empty() ? 1 : std::stoi(argm["t"]);
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update, 60, 1);
#else
    init();
    setTxMode(1);
    printf("Selecting Tx protocol %d\n", txProtocol);
    switch (txProtocol) {
        case 0:
            {
                printf("Using 'Normal' Tx Protocol\n");
                setParameters(1, 40, 9, 3, 0, 50);
            }
            break;
        case 1:
            {
                printf("Using 'Fast' Tx Protocol\n");
                setParameters(1, 40, 6, 3, 0, 50);
            }
            break;
        case 2:
            {
                printf("Using 'Fastest' Tx Protocol\n");
                setParameters(1, 40, 3, 3, 0, 50);
            }
            break;
        case 3:
            {
                printf("Using 'Ultrasonic' Tx Protocol\n");
                setParameters(1, 320, 9, 3, 0, 50);
            }
            break;
        default:
            {
                printf("Using 'Fast' Tx Protocol\n");
                setParameters(1, 40, 6, 3, 0, 50);
            }
    };
    printf("\n");
    std::thread inputThread([]() {
        while (true) {
            std::string input;
            std::cout << "Enter text: ";
            getline(std::cin, input);
            setText(input.size(), input.data());
            std::cout << "Sending ... " << std::endl;
        }
    });

    while (true) {
        SDL_Delay(1);
        update();
    }

    inputThread.join();
#endif

    delete g_data;

    SDL_PauseAudioDevice(devid_in, 1);
    SDL_CloseAudioDevice(devid_in);
    SDL_PauseAudioDevice(devid_out, 1);
    SDL_CloseAudioDevice(devid_out);
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
