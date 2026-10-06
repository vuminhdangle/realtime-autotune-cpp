# Real-Time C++ Autotune & Pitch Shifter - Autotune & Modulator de Hauteur Temps Réel en C++

[En francais plus bas]

A soft real-time audio processing system that performs live pitch detection and pitch correction (autotune) for singing voices. 

This project was developed as part of the Real-time Signal Processing (TSTR) course at Grenoble INP - Phelma under the supervision of Prof. Olivier Perrotin and Thomas Hueber. Built in C++ using the RtAudio framework, time-domain Autocorrelation, FFT harmonic analysis. and Additive Synthesis with Overlap-Add (OLA). 

**Grade: 19/20.**

[Version francaise]

Un système de traitement audio en temps réel ("soft real-time") réalisant la détection de hauteur (F0) et la correction d'intonation en direct pour la voix chantée. 

Ce projet a été réalisé dans le cadre du cours Traitement du Signal Temps Réel (TSTR) à Grenoble INP - Phelma, sous la direction du Prof. Olivier Perrotin et de Thomas Hueber. Développé en C++ avec l'API RtAudio, l'autocorrélation, l'analyse harmonique par FFT et la synthèse additive avec Overlap-Add (OLA).

**Note obtenue : 19/20.**

## Getting Started with the RtAudio API

### 1. Basic API Functionality

#### 1.1. Installing the required dependencies

First, the basic complilation tools were installed:

```bash
sudo apt update
sudo apt install build-essential
```

A dedicated Conda environment was then created for the RtAudio project:

```bash
conda create -n rtaudio python=3.10 gxx_linux-64 alsa-lib -c conda-forge -y
```

The ALSA and PulseAudio-related libraries were also installed:

```bash
conda activate rtaudio
conda install -c conda-forge alsa-lib alsa-plugins pulseaudio
```

The rtaudio Conda environment is used to isolate the dependencies required by the project. 

### 1.2. Compiling RtAudio

The RtAudio 6.0.1 source code was extracted into the project directory. 

From the root directory of RtAudio, the library was configured and complied using:

```bash
./configure
make all
```

After compilation, several test programs are avalable in the tests directory, including `audioproble` and `duplex`.

### 1.3. Detecting available audio devices

The `audioprobe` example was used to check the available audio devices and the audio APIs supported by the system:

```bash
./audioprobe
```

In my laptop, the program detects 11 audio devices. RtAudio was compiled with the following API:
```bash
Compiled APIs:
0. ALSA (alsa)
```

The most relevant device for this project is the device 6:

```bash
Device Name = HDA Intel PCH (ALC256 Analog)
Device Index = 6
Output Channels = 2
Input Channels = 2
Duplex Channels = 2
```

This device supports both audio input and output, making it suitable for a full-duplex real-time audio test.

The device supports the following sample rates: 44100 Hz and 48000 Hz with a preferred sample rate of 48000 Hz. 

### 1.4. Testing real-time audio input and output

After identifying the available audio devices, the duplex example was used to test simultaneous audio input and output with the selected device. 

The syntax of the duplex program is:

```bash
./duplex N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset>
```
where `N` is the number of channels, `fs` is the sampling rate, `iDevice` and `oDevice` are the input and output device indices, and the last two parameters specify the input and output channel offsets.

The `duplex` example was therefore launched using one channel at a sampling rate of 44100 Hz:

```bash
./duplex 1 44100 6 6 0 0
```

The parameters are:
```text
1      > one audio channel
44100  > sampling rate of 44100 Hz
6      > input device index
6      > output device index
0      > input channel offset
0      > output channel offset
```

The program succesfully opened the audio stream and started running in real time:

```text
Stream latency = 0 frames

Running ... press <enter> to quit (buffer frames = 512).
streamTime = 1.01007
streamTime = 2.00853
streamTime = 3.00698
streamTime = 4.00544
streamTime = 5.0039
streamTime = 6.00236
streamTime = 7.00082
```

This test was able to run continuously without reporting an audio device or stream error. The buffer size used by the example was 512 frames. At a sampling rate of 44100 Hz, this corresponds to approximately: 
```text
512 / 44100 \approx 11.6ms
```
of audio per buffer.

This confirms that RtAudio can successfully access the laptop's microphone and audio output in full-duplex mode. This configurations provides the basic audio I/O required for the real-time pitch detection and pitch-shifting algorithms developed in the following sections. 

[continue]
