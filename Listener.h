#ifndef LISTENER_H
#define LISTENER_H

#include <algorithm>
#include <audioclient.h>
#include <cmath>
#include <complex>
#include <endpointvolume.h>
#include <fftw3.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <vector>
#include <windows.h>

__CRT_UUID_DECL(IAudioMeterInformation, 0xC02216F6, 0x8C67, 0x4B5B, 0x9D, 0x00,
                0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64);

MIDL_INTERFACE("C02216F6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *pfPeak) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *
                                                            pnChannelCount) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(
      UINT32 u32ChannelCount, float *afPeakValues) = 0;

  virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(
      DWORD * pdwHardwareSupportMask) = 0;
};

class Point {
public:
  inline Point(float a, float b) {
    x = a;
    y = b;
  }
  inline Point() { x = y = 0.f; }
  float x;
  float y;
};

class Listener {
private:
  IMMDeviceEnumerator *pDeviceEnumerator = nullptr;
  IMMDevice *pAudioRenderDevice = nullptr;
  IAudioMeterInformation *pMeterInformation = nullptr;

  IMMDevice *pAudioCaptureDevice = nullptr;
  IAudioClient *pAudioClient = nullptr;
  WAVEFORMATEX *pMixFormat = nullptr;
  IAudioCaptureClient *pCaptureClient = nullptr;
  fftwf_plan plan;
  fftwf_complex *in;
  fftwf_complex *out;
  size_t ioSize;

  int sampleRate;
  int frequencyBins = 25;
  float availableFrequencies;
  float minFrequency = 20.0f;
  float maxFrequency = 20000.0f;
  float ratio = maxFrequency / minFrequency;
  float maxVolume;

  UINT32 bufferSize;
  UINT32 bytesPerFrame;
  UINT32 numFramesRetrieved;
  UINT32 packetLength = 0;
  BYTE *pBufferData;
  DWORD flags;

  bool audioIsPlaying();
  void generatePlan();

public:
  void getAudioLevel(float *out);
  std::vector<float> getFrequencyData();
  void initialize();
  Listener();
  Listener(int _frequencyBins);
  ~Listener();
};

#endif