#ifndef DECODER_H
#define DECODER_H

#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <pthread.h>

//raylib expect specific audio format, so we need to convert the decoded audio to that format.
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 2
#define SECONDS_BUFFER 4 // buffer 4 seconds of audio
#define BUFFER_SIZE (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * SECONDS_BUFFER)   



typedef struct {
    const char *filepath;
    AVFormatContext *format_ctx;
    AVCodecContext *codec_ctx;
    SwrContext *swr_ctx; // For resampling and format conversion
    int audio_stream_index;
    bool is_playing;
    bool should_exit;
} AudioPlayerState;


// ring buffer struct
//decoder writes, main reads

typedef struct 
{
    float buffer[BUFFER_SIZE];
    int write_ptr;
    int read_ptr;
    pthread_mutex_t buffer_mutex;

} RingBuffer;


bool init_decoder(AudioPlayerState *state, const char *filename);
void decode_audio_frame(AudioPlayerState *state, RingBuffer *rb);
void close_decoder(AudioPlayerState *state);

#endif // DECODER_H