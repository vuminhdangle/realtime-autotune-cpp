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

### 2. Implementation and Usage

#### 2.1. Integrating the Source Files

The source files `duplex.cpp` and `somefunc_2025.cpp` are provided in the `code` folder. The `somefunc_2025.cpp` file has not been modified. 

To integrate the source files into RtAudio:

1. Copy `duplex.cpp` and `somefunc_2025.cpp` into the `rtaudio-6.0.1/tests/` directory. 
2. Create an `execution_output` directory inside `tests/`. This directory is used to store the text files containing the generated samples. 

```bash
cd /path/to/rtaudio-6.0.1/tests
mkdir -p execution_output
```

#### 2.2. Compilation


After compying the source files into the `tests` directory, navigate to this directory and compile the program using `make`:

```bash
cd /path/to/rtaudio-6.0.1/tests
make
```

Make sure that the appropriate build configuration includes the modified `duplex.cpp` source file. 


#### 2.3. Running the Application

The `duplex` program accepts several command-line arguments, described below in the order in which they appear:

| Parameter | Description |
| --- | --- |
| `N` | Number of audio channels |
| `fs` | Sampling rate (use `44100` Hz) |
| `iDevice` | Input audio device index |
| `oDevice` | Output audio device index |
| `iChannelOffset` | Optional input channel offset (use `0` or omit if supported) |
| `oChannelOffset` | Optional output channel offset (use `0` or omit if supported) |
| `file_path` | Optional absolute path to a `.bin` file containing input samples. When provided, the input audio device is ignored. |

**Using a microphone as the input source**

To use a microphone for real-time audio input, run:

```bash
./duplex 1 44100 1 0 0
```

**Playing back samples from a binary file**

To use a `.bin` file as the input source, run:

```bash
./duplex 1 44100 1 0 0 0 ~path/to/audio_files/F01_a3_s100_v04.bin
```

Replace the example file path with the absolute path to your input file. 

#### 2.4. Runtime Controls

The target frequency ($f_0$) and processing mode can be adjusted while the program is running. To change the mode, enter a numerical value in the terminal and press Enter. 

- **Fixed-frequency mode** (`value` > 12): Sets the target frequency to the specified value in Hz. For example, entering 440 sets the target frequency to 440 Hz. 
- **Autotune mode** (0 < value <= 12): Enables semitone quantization of the detected pitch. The entered integer controls the rounding logic ussed to select the target semitone interval. 
- **Pass-through mode** (value <= 0): Disables the frequency override, allowing the synthesis to follow the original detected pitch. 
- **Quit** (q): Enter q and press Enter to terminate the program and save the data. 

#### 2.5. Output Data and Debugging

##### 2.5.1. Audio Files Used in the Report

For each exercise involving plots or audio synthesis, the corresponding raw signal data has been converted to WAV format and stored in the files directory. 

The directory structure is organized as follows: 

```text
files/
└── [exercise_number]/
    └── [signal_type]/
        ├── samples_in.wav
        └── samples_out.wav
```

For example, the files for exercise q14 and the voice signal are stored in `files/q14/voice/`.
- `sample_in.wav`: Original input signal used for the analysis. 
- `sample_out.wav`: Processed output signal. 

##### 2.5.2. Generated Samples and Diagnostic Data

The program saves the following data during execution. The most important files contain the input and output samples, while the remaining files are primarily used for debugging and analysis. 

**Input and output samples**
- `samples_in.txt`: Input samples.
- `samples_out.txt`: Output samples. 

**Pitch tracking**

- `f0.txt`: Detected fundamental frequency $f_0$ throughout the recording. 
- `f0_wanted.txt`: Target fundamental frequency throughout the recording. 

**Harmonic and frequency analysis**
- `amplitude_harmonics.txt`: Amplitudes of the harmonics from the last processing buffer.
- `autocorrelation.txt`: Autocorrelation data from the last processing buffer.
- `fft_real.txt`: Real component of the FFT output from the last processing buffer.
- `fft_imag.txt`: Imaginary component of the FFT output from the last processing buffer.

**Processing buffers and intermediate data**
- `outring.txt`: Circular buffer containing the last second of output samples, used for debugging. 
- `procbuf_create.txt`: Processing buffer captured before program shutdown, after it has been filled with samples. 
- `procbuf_after_standarize.txt`: Processing buffer captured before program shutdown, after input signal normalization. 
- `procbuf_resampled.txt`: Processing buffer captured before program shutdown, after additive synthesis. 

[end]
















