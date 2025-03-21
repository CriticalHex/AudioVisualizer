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

  hr =
      pAudioRenderDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL,
                                   NULL, (void **)&pMeterInformation);
  if (FAILED(hr)) {
    cout << "Failed to activate meterInformation device" << endl;
  }

  // hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole,
  //                                                 &pAudioCaptureDevice);
  // if (FAILED(hr)) {
  //   cout << "Failed to get audio capture endpoint" << endl;
  // }

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
    cout << "Failed to get capture client" << endl;
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
  // fftwf_destroy_plan(plan);
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

vector<float> Listener::getFrequencyData() {
  HRESULT hr;
  vector<BYTE> data;
  vector<float> frequencyVolumes(frequencyBins);
  hr = pCaptureClient->GetNextPacketSize(&packetLength);
  if (FAILED(hr)) {
    cout << "Failed to get the next packet size" << endl;
  }
  while (audioIsPlaying() && data.size() < bufferSize * 8) {
    // cout << "L: " << packetLength << endl;
    // GetBuffer takes this as INPUT first, how many to read,
    // then as OUTPUT sets it to how many were read.
    numFramesRetrieved = packetLength;
    hr = pCaptureClient->GetBuffer(&pBufferData, &numFramesRetrieved, &flags,
                                   NULL, NULL);
    // cout << "N: " << numFramesRetrieved << endl;
    if (FAILED(hr)) {
      cout << "Failed to get buffer" << endl;
    }
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

  if (data.size() != 0) {
    // cout << "D: " << data.size() << endl;
    size_t numFrames = data.size() / bytesPerFrame;
    // cout << "NF: " << numFrames << endl;

    vector<float> mergedChannelData(numFrames);

    switch (pMixFormat->wBitsPerSample) {
    case 32: {
      // creating new array compressing the BYTES in data to FLOATS, so size
      // shrinks by sizeof(float) = 4
      // now, every float in the new array corresponds to a "sample"
      // of which there are multiple, one per channel, per frame.
      // in the usual 2 channel case, there are 2 per frame.
      float *pFloatData = reinterpret_cast<float *>(data.data());

      for (int i = 0; i < numFrames; i++) {
        float sum = 0.0f;
        for (int channel = 0; channel < pMixFormat->nChannels; channel++) {
          int index = (i * pMixFormat->nChannels) + channel;
          sum += pFloatData[index];
        }
        mergedChannelData[i] = sum / pMixFormat->nChannels;
      }
      break;
    }
    default: {
      // me no wanna :(
      cout << "Only 32 bit audio format has been implemented!" << endl;
      return vector<float>(frequencyBins);
      break;
    }
    }

    // windowing
    for (int i = 0; i < numFrames; ++i) {
      float windowFactor = 0.5f * (1 - cos(2 * M_PI * i / (numFrames - 1)));
      mergedChannelData[i] *= windowFactor;
    }

    // Create FFTW complex input/output arrays
    fftwf_complex *in =
        (fftwf_complex *)(fftwf_malloc(sizeof(fftwf_complex) * numFrames));
    fftwf_complex *out =
        (fftwf_complex *)(fftwf_malloc(sizeof(fftwf_complex) * numFrames));

    // Copy data into FFTW input
    for (int i = 0; i < numFrames; ++i) {
      in[i][0] = mergedChannelData[i];
      in[i][1] = 0;
    }

    fftwf_plan fftPlan =
        fftwf_plan_dft_1d(numFrames, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    fftwf_execute(fftPlan);

    fftwf_destroy_plan(fftPlan);

    float magnitude[numFrames];
    for (int i = 0; i < numFrames; i++) {
      magnitude[i] = abs(out[i]);
    }

    float frequenciesPerIndex = float(sampleRate) / numFrames;

    // int maxIndex = ceil(maxFrequency / frequenciesPerIndex);

    for (int i = 0; i < frequencyBins; i++) {
      // Map index to log space
      float freq = minFrequency *
                   pow(10.f, (float(i) / (frequencyBins - 1)) * log10(ratio));

      // Convert frequency to an index in the FFT magnitude array
      float fftIndex = freq / frequenciesPerIndex;
      int indexLow = floor(fftIndex);
      int indexHigh =
          min(indexLow + 1, (int)numFrames - 1); // Ensure within range

      // Interpolate between neighboring FFT bins
      float fraction = fftIndex - indexLow;
      float volume = (1.0f - fraction) * magnitude[indexLow] +
                     fraction * magnitude[indexHigh];

      float normFreq = (freq - minFrequency) / (maxFrequency - minFrequency);
      float boostFactor = pow(normFreq, 10.f) * 20;

      frequencyVolumes[i] = volume * (1.0f + boostFactor);
    }

    // Normalize volume values
    float currentMax =
        *max_element(frequencyVolumes.begin(), frequencyVolumes.end());
    if (currentMax > maxVolume) {
      maxVolume = currentMax;
    }
    // maxVolume = *max_element(frequencyVolumes.begin(),
    // frequencyVolumes.end());
    if (maxVolume > 0) {
      for (int i = 0; i < frequencyVolumes.size(); ++i) {
        frequencyVolumes[i] /= maxVolume;
      }
    }
  }

  return frequencyVolumes;
}