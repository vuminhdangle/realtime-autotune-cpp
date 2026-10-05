/******************************************/
/*
  duplex.cpp
  by Gary P. Scavone, 2006-2019.
*/
/******************************************/

#include "RtAudio.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <fstream>
#include "somefunc_2025.cpp"

/*
typedef char MY_TYPE;
#define FORMAT RTAUDIO_SINT8
*/

/*
typedef signed short MY_TYPE;
#define FORMAT RTAUDIO_SINT16
*/

/*
typedef S24 MY_TYPE;
#define FORMAT RTAUDIO_SINT24

typedef signed long MY_TYPE;
#define FORMAT RTAUDIO_SINT32

typedef float MY_TYPE;
#define FORMAT RTAUDIO_FLOAT32
*/

typedef double MY_TYPE;
#define FORMAT RTAUDIO_FLOAT64


void usage( void ) {
  // Error function in case of incorrect command-line
  // argument specifications
  std::cout << "\nuseage: duplex N fs <iDevice> <oDevice> <iChannelOffset> <oChannelOffset> <file_path>\n";
  std::cout << "    where N = number of channels,\n";
  std::cout << "    fs = the sample rate,\n";
  std::cout << "    iDevice = optional input device index to use (default = 0),\n";
  std::cout << "    oDevice = optional output device index to use (default = 0),\n";
  std::cout << "    iChannelOffset = an optional input channel offset (default = 0),\n";
  std::cout << "    oChannelOffset = optional output channel offset (default = 0).\n";
  std::cout << "    file_path = optional audio file path used for input. Audio is played in loop, and input device is ignored.\n\n";
  exit( 0 );
}

unsigned int getDeviceIndex( std::vector<std::string> deviceNames, bool isInput = false )
{
  unsigned int i;
  std::string keyHit;
  std::cout << '\n';
  for ( i=0; i<deviceNames.size(); i++ )
    std::cout << "  Device #" << i << ": " << deviceNames[i] << '\n';
  do {
    if ( isInput )
      std::cout << "\nChoose an input device #: ";
    else
      std::cout << "\nChoose an output device #: ";
    std::cin >> i;
  } while ( i >= deviceNames.size() );
  std::getline( std::cin, keyHit );  // used to clear out stdin
  return i;
}

double streamTimePrintIncrement = 1.0; // seconds
double streamTimePrintTime = 1.0; // seconds


struct InOutPassData{
  bool newData = false;
  bool using_file = false;
  // std::vector because i dont like new
  std::vector<double> in_ring_buffer; //ring buffer of input samples, not necessary but nice to have it here to see how input looks like
  std::vector<double> out_ring_buffer; //this actually is useful, output ring buffer samples are stored here

  std::vector<double> audio_buffer; // stores audio file (whole)
  unsigned int audio_buffer_index = 0; //index of last buffer begin read

  unsigned int ring_buffer_size = 0; // ring buffer size - 2048
  unsigned int buffer_size = 0; // in/out buffer size - 512
  unsigned int buffer_index = 0; //where the program should start writing to ring buffers

  // buffers for saving
  std::vector<double> save_buffer_in; // contains in samples
  std::vector<double> save_buffer_out; // contains out samples
  std::vector<double> save_buffer_f0; // contains f0s
  std::vector<double> save_buffer_f0_wanted; //contains f0 that were wanted

  unsigned int save_buffer_n = 0; //size of save buffer
  int save_buffer_i = 0; // where to write signals to save_buffers

  // autotune 
  std::vector<double> process_buffer; //2048 processing sample, star of the algorithm (used as input samples, real vals of fft, output samples)
  unsigned int process_buffer_size = 0; // size of buffer above

  std::vector<double> hanning_window; // hanning window of size of the process buffer
  std::vector<double> autocorrelation; // autocorrelation of size of the process buffer
  double f0 = 0; // f0 calculated from input
  double f0_wanted = 180; // wanted f0 (or snapping to the nearest semitones)

  std::vector<double> im_fft; // imaginary part of fft

  int num_harmonics = 0; // number of harmonics to calculate (300)
  int fs = 0; // sample rate

  std::vector<double> phi_harmonics; // phase of each harmonic - must be here
  std::vector<double> amplitude_harmonics; // amplitude of each harmonics, it musn't be here, but now it doesnt have to be initialized in inout, and it can be nicely written later

  int out_ring_ola_index = 0;
  int out_ring_play_index = 0;

  // process buffer dump vectors
  std::vector<double> proc_dump_buf_create;
  std::vector<double> proc_dump_buf_after_standardize;
  std::vector<double> proc_dump_buf_fft_real;
  std::vector<double> proc_dump_buf_resampled;
  


  // mutex
  std::mutex m;
};

double round_to_semitone(double f0, int semitone_rounding){
  if (f0 <= 10.0 || f0 >= 1500) return f0;

  double f0_st = 12 * std::log2(f0);
  double snapped_st = std::round(f0_st / semitone_rounding) * semitone_rounding;
  return std::pow(2.0, snapped_st / 12.0);

}

void save_to_file(const std::string& filename, const double *data, int size){
  std::ofstream file(filename);
  for (int i = 0; i < size; i++){
    file << data[i] << '\n';
  }
}

int inout( void *outputBuffer, void *inputBuffer, unsigned int /*nBufferFrames*/,
           double streamTime, RtAudioStreamStatus status, void *data )
{
  // Since the number of input and output channels is equal, we can do
  // a simple buffer copy operation here.
  if ( status ) std::cout << "Stream over/underflow detected." << std::endl;

  if ( streamTime >= streamTimePrintTime ) {
    std::cout << "streamTime = " << streamTime << std::endl;
    streamTimePrintTime += streamTimePrintIncrement;
  }

  
  
  double *in = static_cast<double*>(inputBuffer);
  double *out = static_cast<double*>(outputBuffer);

  

  // init user data, lock access to it in other threads
  auto *user_data = static_cast<InOutPassData*>(data);
  std::lock_guard<std::mutex> lock(user_data->m);
  
  // if using file, fill input buffer from audio file buffer
  if (user_data->using_file) {
    int audio_pos_start = user_data->audio_buffer_index;
    int sz_audio_buffer = user_data -> audio_buffer.size();
    
    int dst = 0;
    for (unsigned int i = 0; i < user_data->buffer_size; i++) {
      dst = (audio_pos_start + i) % sz_audio_buffer;
      in[i] = user_data->audio_buffer[dst];
    }
    audio_pos_start = (audio_pos_start + user_data-> buffer_size) % sz_audio_buffer;
    user_data->audio_buffer_index = audio_pos_start;
  }

  
  // processing (now just copy)
  //std::memcpy(out, in, user_data->buffer_size * sizeof(double));

  // processing //

  //1. write to inringbuffer
  unsigned int& in_ring_start_index = user_data -> buffer_index;
  unsigned int& ring_buffer_size = user_data -> ring_buffer_size;
  unsigned int& inout_buffer_size = user_data -> buffer_size;
  std::vector<double> & in_ring_buffer = user_data -> in_ring_buffer;
  unsigned int dst = 0;
  
  
  for (unsigned int i = 0; i < inout_buffer_size; i++){
    dst = (in_ring_start_index + i) % ring_buffer_size;
    in_ring_buffer[dst] = in[i];
  }
  in_ring_start_index = dst;

  
  //2. get 2048 last samples fom inringbuffer to processtable, apply hanning, standardize
  unsigned int& process_buffer_size = user_data -> process_buffer_size;
  std::vector<double> &process_buffer = user_data -> process_buffer;
  std::vector<double> &hanning_window = user_data -> hanning_window;
  
  unsigned int j = (in_ring_start_index + 1 + ring_buffer_size - process_buffer_size) % ring_buffer_size;
  double sum = 0;
  double mean = 0;

  // just for logging)
  user_data -> proc_dump_buf_create = process_buffer;
  

  dst = 0;
  for (unsigned int i = 0; i < process_buffer_size; i++){
    dst = (j + i) % ring_buffer_size;
    process_buffer[i] = in_ring_buffer[dst] * hanning_window[i];
    sum =+ process_buffer[i];
  }
  mean = sum / process_buffer_size;
  for (unsigned int i = 0; i < process_buffer_size; i++) process_buffer[i] -= mean; 
  
  // TODO: remove later (just for logging)
  user_data -> proc_dump_buf_after_standardize = process_buffer;

  //3. autocorrelation
  int &fs = user_data -> fs;
  unsigned int lag_min = static_cast<unsigned int>(fs/1000);
  unsigned int lag_max = static_cast<unsigned int>(fs/45);

  // regarding Q12 - simulate unimplemented lag limits
  // unsigned int lag_min = static_cast<unsigned int>(1);
  // unsigned int lag_max = static_cast<unsigned int>(4294967295);
  // ofc it will not work, as this implementation needs to have lag limit (index 0 will always have biggest val)
  int best_lag = lag_min;
  double best_val = -INFINITY;

  std::vector<double> &autocorrelation = user_data -> autocorrelation;
  for (unsigned int lag = 0; lag < process_buffer_size; lag++){
    double sum = 0.0;
    for (unsigned int i = 0; i + lag < process_buffer_size; i++){
      sum += process_buffer[i] * process_buffer[i + lag];
    }
    autocorrelation[lag] = sum;
    if ((lag > lag_min) && (lag < lag_max)){
      if (autocorrelation[lag] > best_val){
        best_val = autocorrelation[lag];
        best_lag = lag;
      }
    }
  }
  
  double &f0 = user_data -> f0;
  f0 = double(fs) / double(best_lag);
  
  
  //3.1. snap to best harmonic (or not, if not wanted)
  double &f0_fromstr = user_data -> f0_wanted;
  double f0_wanted = 0;
  if (f0_fromstr <= 12 && f0_fromstr > 0){
    f0_wanted = round_to_semitone(f0, static_cast<int>(f0_fromstr));
  }
  else if (f0_fromstr <= 0) f0_wanted = f0;
  else f0_wanted = f0_fromstr;

  
  //4. fft, find peaks, get harmonics
  std::vector<double> &fft_im = user_data -> im_fft;

  if (fftr(process_buffer.data(), fft_im.data(), process_buffer_size)) std::cout << "FFT error" << std::endl;
  const int nyquist_fq_i = (process_buffer_size / 2);

  int &num_harmonics = user_data -> num_harmonics;

  // (just for logging)
  user_data -> proc_dump_buf_fft_real = process_buffer;
  
  //18.a - initialize phase transition vector
  std::vector<double> &phi_harmonics = user_data -> phi_harmonics;
  std::vector<double> &amplitude_harmonics = user_data -> amplitude_harmonics;
  std::vector<double> dphi_harmonics(num_harmonics, 0);

  
  for (int h = 0; h < num_harmonics; h++){
    // find harmonics amplitude
    double fh = (h + 1) * f0;
    double fh_synth = (h + 1) * f0_wanted;
    // if above nyquist frequency - skip (and zero the harmonics, it can contain data if f0 had risen)
    if (fh >= 0.5 * fs || fh_synth >= 0.5 * fs){
      amplitude_harmonics[h] = 0;
      dphi_harmonics[h] = 0;
      continue;
    }
    // find index of harmonics
    int fh_i = std::round(fh * process_buffer_size / fs);
    // search biggest value nearby harmonic
    int search_region = 3;
    int j = std::max(0, fh_i - search_region);
    int j_max = std::min(fh_i + search_region, nyquist_fq_i);
    double Amax = -INFINITY;
    double A = 0;

    for (; j <= j_max; j++){
      // calculate magnitude (only for samples near harmonics, rest is useless)
      // then get max -> this is harmonic amplitude
      A = std::sqrt(process_buffer[j] * process_buffer[j] + fft_im[j] * fft_im[j]);
      if (A > Amax){
        Amax = A;
      }
    }
    // write amplitude of harmonic
    amplitude_harmonics[h] = (Amax * 2.0)/ double(user_data -> process_buffer_size);
    // now get autotuned phase
    dphi_harmonics[h] = 2.0 * M_PI * fh_synth / fs;
  }
  

  //5. frame synthesis
  // workng current_phases
  // 18.a) create new vector based on previous transitions, on this vector next transitions will be calculated
  std::vector<double> current_phases = phi_harmonics;

  // delibarately broken current_phases (for ex.17)
  //std::vector<double> current_phases(num_harmonics, 0);

  unsigned int &buffer_size = user_data -> buffer_size; 

  
  for (unsigned int n = 0; n < process_buffer_size; n++){
    // poor process buffer, got fouriered now zeroed
    process_buffer[n] = 0;
    // sum of harmonics -> nth output of sample
    for (int h = 0; h < num_harmonics; h++){
      // no amplitude (or above nyquist)? - skip
      if (amplitude_harmonics[h] == 0 || dphi_harmonics[h] == 0) continue;
      
      // sum of every harmonic in nth sample
      process_buffer[n] += amplitude_harmonics[h] * std::cos(current_phases[h]);
      // 'move' phase forward by one sample
      current_phases[h] += dphi_harmonics[h];    
    }
    // hannigize (???) the process buffer to remove leakage effect
    process_buffer[n] *= hanning_window[n]; 
  }
  // process buffer now contains new samples to output - furiered, zeroed, now it will be output - what a legend
  user_data -> proc_dump_buf_resampled = process_buffer;
  
  // phase transition by buffer size, to prepare it for next buffer
  for (int h = 0; h < num_harmonics; h++){
    phi_harmonics[h] += dphi_harmonics[h] * buffer_size;
    // limit the value from 0 to 2pi
    phi_harmonics[h] = std::fmod(phi_harmonics[h], 2.0 * M_PI);
  }
  
  //6. ola

  int &out_ring_ola_write_pos = user_data -> out_ring_ola_index;
  int &out_ring_play_pos = user_data -> out_ring_play_index;

  std::vector<double> &out_ring_buffer = user_data -> out_ring_buffer;
  // ola writing
  for (unsigned int n = 0; n < process_buffer_size; n++){
    int ola_index = (out_ring_ola_write_pos + n) % ring_buffer_size;
    // REGARDING Q21 
    // no OLA implementation
    // out_ring_buffer[ola_index] = process_buffer[n];
    // OLA implemented
    out_ring_buffer[ola_index] += process_buffer[n];
  }
  out_ring_ola_write_pos = (out_ring_ola_write_pos + buffer_size) % ring_buffer_size;



  
  //7. get finally output samples
  for (unsigned int n = 0; n < buffer_size; n++){
    int ola_index = (out_ring_play_pos + n) % ring_buffer_size;
    // regarding q21
    // no ola: dont divide by 4
    // out[n] = out_ring_buffer[ola_index];
    // with ola : divide by 4
    out[n] = out_ring_buffer[ola_index] / 4;
    out_ring_buffer[ola_index] = 0;
  }
  out_ring_play_pos = (out_ring_play_pos + buffer_size) % ring_buffer_size;


  
  // add samples to savebuffer
  int save_buffer_size = static_cast<int>(user_data -> buffer_size);
  int save_buffer_n = static_cast<int>(user_data -> save_buffer_n);
  int old_pos = user_data->save_buffer_i;

  // store in and out buffer data
  write_buff_dump(in, 
    save_buffer_size,
    user_data->save_buffer_in.data(),
    save_buffer_n,
    &old_pos
  );
  old_pos = user_data->save_buffer_i;

  write_buff_dump(out,
  save_buffer_size,
  user_data->save_buffer_out.data(),
  save_buffer_n,
  &old_pos
  );
  old_pos = user_data->save_buffer_i;

  // now store f0 and f0 wanted in buffers
  if (f0 > 900.0) f0 = 0.0;
  if (f0_wanted > 900.0) f0_wanted = 0.0;
  std::vector<double> f0_vec(buffer_size, f0);
  std::vector<double> f0_wanted_vec(buffer_size, f0_wanted);

  write_buff_dump(f0_vec.data(),
  save_buffer_size,
  user_data->save_buffer_f0.data(),
  save_buffer_n,
  &old_pos
  );
  old_pos = user_data->save_buffer_i;

  write_buff_dump(f0_wanted_vec.data(),
  save_buffer_size,
  user_data->save_buffer_f0_wanted.data(),
  save_buffer_n,
  &old_pos
  );
  user_data->save_buffer_i = old_pos;
  

  return 0;
}

std::atomic<bool> running{true};
int main( int argc, char *argv[] )
{
  unsigned int channels, fs, bufferBytes, oDevice = 0, iDevice = 0, iOffset = 0, oOffset = 0;
  std::string audio_file_path = "";


  // Minimal command-line checking
  if (argc < 3 || argc > 8 ) usage();

  RtAudio adac;
  std::vector<unsigned int> deviceIds = adac.getDeviceIds();
  if ( deviceIds.size() < 1 ) {
    std::cout << "\nNo audio devices found!\n";
    exit( 1 );
  }

  channels = (unsigned int) atoi(argv[1]);
  fs = (unsigned int) atoi(argv[2]);
  if ( argc > 3 )
    iDevice = (unsigned int) atoi(argv[3]);
  if ( argc > 4 )
    oDevice = (unsigned int) atoi(argv[4]);
  if ( argc > 5 )
    iOffset = (unsigned int) atoi(argv[5]);
  if ( argc > 6 )
    oOffset = (unsigned int) atoi(argv[6]);
  if ( argc > 7 )
    audio_file_path = argv[7];
  

  // Let RtAudio print messages to stderr.
  adac.showWarnings( true );

  // Set the same number of channels for both input and output.
  unsigned int bufferFrames = 512;


  RtAudio::StreamParameters iParams, oParams;
  iParams.nChannels = channels;
  iParams.firstChannel = iOffset;
  oParams.nChannels = channels;
  oParams.firstChannel = oOffset;

  if ( iDevice == 0 )
    iParams.deviceId = adac.getDefaultInputDevice();
  else {
    if ( iDevice >= deviceIds.size() )
      iDevice = getDeviceIndex( adac.getDeviceNames(), true );
    iParams.deviceId = deviceIds[iDevice];
  }
  if ( oDevice == 0 )
    oParams.deviceId = adac.getDefaultOutputDevice();
  else {
    if ( oDevice >= deviceIds.size() )
      oDevice = getDeviceIndex( adac.getDeviceNames() );
    oParams.deviceId = deviceIds[oDevice];
  }

  //////////////////////////////////
  // Program init
  const unsigned int t_buf_seconds = 1;
  const unsigned int t_savebuf_seconds = 30;
  const unsigned int TOTAL_SAMPLES = fs * t_buf_seconds;
  const unsigned int TOTAL_SAVEBUFFER_SAMPLES = fs * t_savebuf_seconds;
  const unsigned int PROCESSBUFFER_SIZE = 2048;
  const unsigned int NUM_HARMONICS = 300;

  std::cout << "Total samples in ring buffer: " << TOTAL_SAMPLES << std::endl;
  
  // INIT THE INOUT-MAIN COMMUNICATION STRUCT
  InOutPassData inout_user_data;
  inout_user_data.buffer_size = bufferFrames;
  inout_user_data.using_file = false;

  inout_user_data.in_ring_buffer.resize(TOTAL_SAMPLES);
  inout_user_data.out_ring_buffer.resize(TOTAL_SAMPLES);
  inout_user_data.ring_buffer_size = TOTAL_SAMPLES;

  inout_user_data.save_buffer_in.resize(TOTAL_SAVEBUFFER_SAMPLES);
  inout_user_data.save_buffer_out.resize(TOTAL_SAVEBUFFER_SAMPLES);
  inout_user_data.save_buffer_f0.resize(TOTAL_SAVEBUFFER_SAMPLES);
  inout_user_data.save_buffer_f0_wanted.resize(TOTAL_SAVEBUFFER_SAMPLES);
  inout_user_data.save_buffer_n = TOTAL_SAVEBUFFER_SAMPLES;

  inout_user_data.process_buffer.resize(PROCESSBUFFER_SIZE);
  inout_user_data.process_buffer_size = PROCESSBUFFER_SIZE;

  inout_user_data.hanning_window.resize(PROCESSBUFFER_SIZE);
  hanning(inout_user_data.hanning_window.data(), static_cast<int>(PROCESSBUFFER_SIZE));

  inout_user_data.autocorrelation.resize(PROCESSBUFFER_SIZE);

  inout_user_data.im_fft.resize(PROCESSBUFFER_SIZE);
  
  inout_user_data.fs = fs;

  inout_user_data.num_harmonics = NUM_HARMONICS;
  inout_user_data.phi_harmonics.resize(NUM_HARMONICS);
  inout_user_data.phi_harmonics.assign(NUM_HARMONICS, 0);
  inout_user_data.amplitude_harmonics.resize(NUM_HARMONICS);
  inout_user_data.amplitude_harmonics.assign(NUM_HARMONICS, 0);

  // process buffer dumps
  inout_user_data.proc_dump_buf_create.resize(PROCESSBUFFER_SIZE);
  inout_user_data.proc_dump_buf_after_standardize.resize(PROCESSBUFFER_SIZE);
  inout_user_data.proc_dump_buf_fft_real.resize(PROCESSBUFFER_SIZE);
  inout_user_data.proc_dump_buf_resampled.resize(PROCESSBUFFER_SIZE);
  
   
  // LOAD AUDIO FILE IF SPECIFIED
  if (audio_file_path != "") {
    inout_user_data.using_file = true;
    // load audio file
    std::cout << "Using audio file: " << audio_file_path << std::endl;

    std::ifstream f(audio_file_path, std::ios::binary);
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::cout << "File size: " << size << " bytes." << std::endl;

    std::size_t samples_num = static_cast<std::size_t>(size / sizeof(double));
    std::cout << "Samples in file: " << samples_num << std::endl;

    std::vector<double> audio_data(samples_num);

    if (f.read(reinterpret_cast<char*>(audio_data.data()), size)) {
      std::cout << "Successfully read audio data from file." << std::endl;
      inout_user_data.audio_buffer = audio_data;
      inout_user_data.using_file = true;
    } else {
      std::cerr << "Error reading audio data from file." << std::endl;
      return 1;
    }

  }

  //variable used to display the f0 in console
  //must be here because later there is goto
  double f0_m = 0;
  //blocking/input loop - user input is processed here 
  std::thread input_thread([&] {
    std::cout << "Press 'q' + Enter to quit\n";
    std::string line;
    
    while (std::getline(std::cin, line)) {
      
        if (line == "q") {
            running = false;
            break;
        }
        try {
          double new_f0 = std::stod(line);
          {
            std::lock_guard<std::mutex> lock(inout_user_data.m);
            inout_user_data.f0_wanted = new_f0;
          }
          std::cout << "f0 set to " << new_f0 << std::endl;
        }
        catch (const std::exception&){
          std::cout << "wrong input" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });

  ///////////////////////// end of program init, begin RtAudio stream setup

  RtAudio::StreamOptions options;
  //options.flags |= RTAUDIO_NONINTERLEAVED;
  bufferBytes = bufferFrames * channels * sizeof( MY_TYPE );
  if ( adac.openStream( &oParams, &iParams, FORMAT, fs, &bufferFrames, &inout, &inout_user_data, &options ) ) {
    goto cleanup;
  }

  if ( adac.isStreamOpen() == false ) goto cleanup;

  // Test RtAudio functionality for reporting latency.
  std::cout << "\nStream latency = " << adac.getStreamLatency() << " frames" << std::endl;

  if ( adac.startStream() ) goto cleanup;
  
  // rtstuio init complete

  // output loop, and main loop

  while (running) {
    //read values from inout in other thread, but to not break the inout_user_data, lock it
  {
    std::lock_guard<std::mutex> lock(inout_user_data.m);
    f0_m = inout_user_data.f0;
  }
  // display f0 every 1s
  std::cout << f0_m << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
 }
 // if `running` was broken in previous thread loop, join threads and begin to deinit the program
 if (input_thread.joinable())
    input_thread.join();


  // Stop the stream.
  if ( adac.isStreamRunning() )
    adac.stopStream();

 cleanup:
  if ( adac.isStreamOpen() ) adac.closeStream();

  std::cout << "\n saving stored buffers...\n";

  InOutPassData &io = inout_user_data;

  int sz_save_buffer = io.save_buffer_i;
  int sz_proc_buffer = io.process_buffer_size;
  int sz_ring_buffer = io.ring_buffer_size;
  int sz_harmonics   = io.num_harmonics;

  // Save using helper function into the output folder
  save_to_file("execution_output/samples_in.txt",        io.save_buffer_in.data(),      sz_save_buffer);
  save_to_file("execution_output/samples_out.txt",       io.save_buffer_out.data(),     sz_save_buffer);
  save_to_file("execution_output/f0.txt",                io.save_buffer_f0.data(),      sz_save_buffer);
  save_to_file("execution_output/f0_wanted.txt",         io.save_buffer_f0_wanted.data(), sz_save_buffer);

  save_to_file("execution_output/autocorrelation.txt",   io.autocorrelation.data(),     sz_proc_buffer);
  save_to_file("execution_output/amplitude_harmonics.txt", io.amplitude_harmonics.data(), sz_harmonics);
  save_to_file("execution_output/outring.txt",           io.out_ring_buffer.data(),     sz_ring_buffer);

  save_to_file("execution_output/fft_imag.txt", io.im_fft.data(), sz_proc_buffer);
  save_to_file("execution_output/fft_real.txt", io.proc_dump_buf_fft_real.data(), sz_proc_buffer);
  save_to_file("execution_output/procbuf_create.txt", io.proc_dump_buf_create.data(), sz_proc_buffer);
  save_to_file("execution_output/procbuf_after_standardize.txt", io.proc_dump_buf_after_standardize.data(), sz_proc_buffer);
  save_to_file("execution_output/procbuf_resampled.txt", io.proc_dump_buf_resampled.data(), sz_proc_buffer);

  
  


  return 0;
}
