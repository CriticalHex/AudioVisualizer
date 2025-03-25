#include "Listener.h"

using namespace std;

void Listener::initialize() {
  HRESULT hr;

  hr = CoInitialize(NULL);
  if (FAILED(hr)) {
    cout << "Failed to initialize" << endl;
  }

  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator),
                        (void **)&pDeviceEnumerator);
  if (FAILED(hr)) {
    cout << "Failed to create device enumerator" << endl;
  }

  hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
                                                  &pAudioRenderDevice);
  if (FAILED(hr)) {

    cout << "Failed to get the audio render endpoint" << endl;
  }

  // if you want to read from the mic, instead of system audio
  // hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
  //                                                 &pAudioCaptureDevice);
  // if (FAILED(hr)) {
  //   cout << "Failed to get audio capture endpoint" << endl;
  // }

  hr =
      pAudioRenderDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL,
                                   NULL, (void **)&pMeterInformation);
  if (FAILED(hr)) {
    cout << "Failed to activate meterInformation device" << endl;
  }

  hr = pAudioRenderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                                    (void **)&pAudioClient);
  if (FAILED(hr)) {
    cout << "Failed to activate audio client" << endl;
  }

  hr = pAudioClient->GetMixFormat(&pMixFormat);
  if (FAILED(hr)) {
    cout << "Failed to set audio format" << endl;
  } else {
    cout << "Bits: " << pMixFormat->wBitsPerSample << endl;
    cout << "Sample Rate: " << pMixFormat->nSamplesPerSec << endl;
    cout << "Channels: " << pMixFormat->nChannels << endl;

    sampleRate = pMixFormat->nSamplesPerSec;
    availableFrequencies = float(sampleRate) / 2;
  }

  hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pMixFormat,
                                nullptr);
  if (FAILED(hr)) {
    cout << "Failed to set audio format" << endl;
  }

  hr = pAudioClient->GetBufferSize(&bufferSize);
  if (FAILED(hr)) {
    cout << "Failed to get buffer size" << endl;
  } else {
    cout << "Buffer size: " << bufferSize << endl;
  }

  hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient),
                                (void **)&pCaptureClient);
  if (FAILED(hr)) {
    cout << "Failed to get capture client" << endl;
    switch (hr) {
    case E_POINTER:
      cout << "Parameter ppv is NULL." << endl;
      break;
    case E_NOINTERFACE:
      cout << "The requested interface is not available." << endl;
      break;
    case AUDCLNT_E_NOT_INITIALIZED:
      cout << "The audio stream has not been initialized." << endl;
      break;
    case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
      cout << "The caller tried to access an IAudioCaptureClient interface on "
              "a rendering endpoint, or an IAudioRenderClient interface on a "
              "capture endpoint."
           << endl;
      break;
    case AUDCLNT_E_DEVICE_INVALIDATED:
      cout << "The audio endpoint device has been unplugged, or the audio "
              "hardware or associated hardware resources have been "
              "reconfigured, disabled, removed, or otherwise made unavailable "
              "for use."
           << endl;
      break;
    case AUDCLNT_E_SERVICE_NOT_RUNNING:
      cout << "The Windows audio service is not running." << endl;
      break;
    }
  }

  hr = pAudioClient->Start();
  if (FAILED(hr)) {
    cout << "Failed to start audio client" << endl;
  }

  bytesPerFrame = (pMixFormat->wBitsPerSample / 8) * pMixFormat->nChannels;
  cout << "Bytes per Frame: " << bytesPerFrame << endl;
}

Listener::Listener() { initialize(); }

Listener::Listener(int _frequencyDataSize) {
  initialize();
  frequencyBins = _frequencyDataSize;
}

Listener::~Listener() {
  pMeterInformation->Release();
  pAudioRenderDevice->Release();
  pAudioCaptureDevice->Release();
  pDeviceEnumerator->Release();
  pAudioClient->Stop();
  pAudioClient->Release();
  pCaptureClient->Release();

  CoUninitialize();
}

void Listener::getAudioLevel(float *volume) {
  pMeterInformation->GetPeakValue(volume);
}

bool Listener::audioIsPlaying() {
  float volume;
  pMeterInformation->GetPeakValue(&volume);
  return volume > .00001f;
}

float abs(fftwf_complex v) { return sqrt(v[0] * v[0] + v[1] * v[1]); }

float lerp(float start, float end, float t) {
  return start + t * (end - start);
}

void Listener::fillBuffer(vector<BYTE> &data) {
  HRESULT hr;
  hr = pCaptureClient->GetNextPacketSize(&packetLength);
  if (FAILED(hr)) {
    cout << "Failed to get the next packet size" << endl;
  }
  while (audioIsPlaying() && data.size() < bufferSize * 8) {
    // GetBuffer takes this as INPUT first, how many to read,
    // then as OUTPUT sets it to how many were read.
    numFramesRetrieved = packetLength;
    hr = pCaptureClient->GetBuffer(&pBufferData, &numFramesRetrieved, &flags,
                                   NULL, NULL);
    if (FAILED(hr)) {
      cout << "Failed to get buffer" << endl;
    }
    // you can get if the audio was silent like this,
    // but it doesn't seem to work very well.
    // if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
    //   cout << "Got silent" << endl;
    // }

    data.insert(data.end(), pBufferData,
                pBufferData + numFramesRetrieved * bytesPerFrame);

    hr = pCaptureClient->ReleaseBuffer(numFramesRetrieved);
    if (FAILED(hr)) {
      cout << "Failed to release the buffer" << endl;
    }

    hr = pCaptureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr)) {
      cout << "Failed to get the next packet size" << endl;
    }
  }
}

vector<float> Listener::mergeChannels(vector<BYTE> &data, size_t frames) {
  vector<float> mergedChannelData(frames);

  switch (pMixFormat->wBitsPerSample) {
  case 32: {
    // creating new array compressing the BYTES in data to FLOATS, so size
    // shrinks by sizeof(float) = 4
    // now, every float in the new array corresponds to a "sample"
    // of which there are multiple, one per channel, per frame.
    // in the usual 2 channel case, there are 2 per frame.
    float *pFloatData = reinterpret_cast<float *>(data.data());

    // because of the above cast, every two floats in the array is the two
    // channels so this takes every two floats and averages them, because I'm
    // not doing a surround sound visualization
    for (int i = 0; i < frames; i++) {
      float sum = 0.0f;
      for (int channel = 0; channel < pMixFormat->nChannels; channel++) {
        int index = (i * pMixFormat->nChannels) + channel;
        sum += pFloatData[index];
      }
      mergedChannelData[i] = sum / pMixFormat->nChannels;
    }
    return mergedChannelData;
  }
  default: {
    // me no wanna :(
    cout << "Only 32 bit audio format has been implemented!" << endl;
    return vector<float>(frequencyBins);
  }
  }
}

void window(vector<float> &data, size_t frames) {
  // windowing, makes data better for the fast fourier transform
  for (int i = 0; i < frames; ++i) {
    float windowFactor = 0.5f * (1 - cos(2 * M_PI * i / (frames - 1)));
    data[i] *= windowFactor;
  }
}

void fft(vector<float> amplitudeOverTime, size_t frames,
         float *amplitudePerFrequency) {
  fftwf_complex *in =
      (fftwf_complex *)(fftwf_malloc(sizeof(fftwf_complex) * frames));
  fftwf_complex *out =
      (fftwf_complex *)(fftwf_malloc(sizeof(fftwf_complex) * frames));

  for (int i = 0; i < frames; ++i) {
    in[i][0] = amplitudeOverTime[i];
    in[i][1] = 0;
  }

  fftwf_plan fftPlan =
      fftwf_plan_dft_1d(frames, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  // excecute the fast fourier transform, converts data from the time domain
  // recorded by the system, into data in the frequency domain. The time
  // domain is amplitude over time, and the frequency domain is amplitude over
  // frequency, as in, the amplitude of each frequency in the captured time.
  // The longer you listen for, the more granular that data is. With a sample
  // rate of 44kHz, this program captures amplitude for each group of 44Hz.
  // This could be as granular as an amplitude for each frequency, but it
  // requires you to listen for too long, the frame rate is like a frame every
  // 3 seconds
  fftwf_execute(fftPlan);

  fftwf_destroy_plan(fftPlan);

  // the data is in complex numbers, so this gets the absolute value of them
  for (int i = 0; i < frames; i++) {
    amplitudePerFrequency[i] = abs(out[i]);
  }
}

vector<float> Listener::mapDataToFrequencyBins(float *amplitudePerFrequency,
                                               size_t frames) {
  // this is about 44Hz for a sample rate of 44kHz
  float frequenciesPerIndex = float(sampleRate) / frames;
  vector<float> frequencyVolumes(frequencyBins);

  for (int i = 0; i < frequencyBins; i++) {
    // Map index to log space
    float freq = minFrequency *
                 pow(10.f, (float(i) / (frequencyBins - 1)) * log10(ratio));

    // Convert frequency to an index in the FFT magnitude array
    float fftIndex = freq / frequenciesPerIndex;
    int indexLow = floor(fftIndex);
    // Ensure within range
    int indexHigh = min(indexLow + 1, (int)frames - 1);
    // Interpolate between neighboring FFT bins
    float fraction = fftIndex - indexLow;
    frequencyVolumes[i] = (1.0f - fraction) * amplitudePerFrequency[indexLow] +
                          fraction * amplitudePerFrequency[indexHigh];
  }

  return frequencyVolumes;
}

void Listener::normalizeVolume(vector<float> &volumes) {
  // Normalize volume values
  // this one normalizes to the loudest volume heard since the beginning of
  // the program
  float currentMax = *max_element(volumes.begin(), volumes.end());
  if (currentMax > maxVolume) {
    maxVolume = currentMax;
  } else {
    // if we aren't finding louder volumes, reduce the max slowly to prevent a
    // single dominant volume that occurred in a different song for example
    maxVolume *= .999f;
  }

  // while this one normalizes to the loudest volume this frame
  // maxVolume = *max_element(frequencyVolumes.begin(),
  // frequencyVolumes.end());

  // this style of normalization is for the sake of accentuating the higher
  // frequencies, as they are usually of lower magnitude than the low
  // frequencies, and with linear normalization, they are overpowered.
  // Granted, this is *barely* non-linear, but it makes a big difference
  float alpha = .02f;
  if (maxVolume > 0) {
    for (int i = 0; i < volumes.size(); ++i) {
      volumes[i] = log1p(alpha * volumes[i]) / log1p(alpha * maxVolume);
    }
  }
}

vector<float> Listener::getFrequencyData() {

  vector<BYTE> amplitudeOverTime;
  fillBuffer(amplitudeOverTime);

  if (amplitudeOverTime.size() != 0) {
    size_t frames = amplitudeOverTime.size() / bytesPerFrame;

    vector<float> mergedAmplitudeOverTime =
        mergeChannels(amplitudeOverTime, frames);
    window(mergedAmplitudeOverTime, frames);

    float amplitudePerFrequency[frames];
    fft(mergedAmplitudeOverTime, frames, amplitudePerFrequency);

    vector<float> volumePerFrequency =
        mapDataToFrequencyBins(amplitudePerFrequency, frames);

    normalizeVolume(volumePerFrequency);
    return volumePerFrequency;
  }

  return vector<float>(frequencyBins);
}