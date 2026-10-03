# Real-Time C++ Autotune & Pitch Shifter - Autotune & Modulator de Hauteur Temps Réel en C++

[En francais plus bas]

A soft real-time audio processing system that performs live pitch detection and pitch correction (autotune) for singing voices. 

This project was developed as part of the Real-time Signal Processing (TSTR) course at Grenoble INP - Phelma under the supervision of Prof. Olivier Perrotin and Thomas Hueber. Built in C++ using the RtAudio framework, time-domain Autocorrelation, FFT harmonic analysis. and Additive Synthesis with Overlap-Add (OLA). 

[Version francaise]

Un système de traitement audio en temps réel ("soft real-time") réalisant la détection de hauteur (F0) et la correction d'intonation en direct pour la voix chantée. 

Ce projet a été réalisé dans le cadre du cours Traitement du Signal Temps Réel (TSTR) à Grenoble INP - Phelma, sous la direction du Prof. Olivier Perrotin et de Thomas Hueber. Développé en C++ avec l'API RtAudio, l'autocorrélation, l'analyse harmonique par FFT et la synthèse additive avec Overlap-Add (OLA).

## Getting Started with the RtAudio API

### 1. Basic API Functionality

#### 1.1. Installing the required dependencies

First, the basic complilation tools were installed:

'''bash
sudo apt update
sudo apt install build-essential
'''

A dedicated Conda environment was then created for the RtAudio project:

'''bash
conda create -n rtaudio python=3.10 gxx_linux-64 alsa-lib -c conda-forge -y
'''

The ALSA and PulseAudio-related libraries were also installed:

'''bash
conda activate rtaudio
conda install -c conda-forge alsa-lib alsa-plugins pulseaudio
'''

The rtaudio Conda environment is used to isolate the dependencies required by the project. 

### 1.2. Compiling RtAudio

The RtAudio 6.0.1 source code was extracted into the project directory. 

From the root directory of RtAudio, the library was configured and complied using:

'''bash
./configure
make all
'''


