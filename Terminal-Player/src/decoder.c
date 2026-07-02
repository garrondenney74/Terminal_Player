#include "decoder.h"
#include <stdio.h>
#include <stdint.h>



//initialize decoder
bool init_decoder( AudioPlayerState *state, const char *filename ) {

    state->filepath = filename;
    state->is_playing = false;
    state->should_exit = false;

    // Open file and read header
    if (avformat_open_input(&state->format_ctx, filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", filename);
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(state->format_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return false;
    }

    // Find the audio stream
    state->audio_stream_index = av_find_best_stream(state->format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (state->audio_stream_index < 0) {
        fprintf(stderr, "Could not find audio stream in file\n");
        return false;
    }

    // Find decoder for the stream
    AVStream *stream = state->format_ctx->streams[state->audio_stream_index];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Failed to find codec\n");
        return false;
    }

    // Allocate a codec context
    state->codec_ctx = avcodec_alloc_context3(codec);
    if (!state->codec_ctx) {
        fprintf(stderr, "Failed to allocate the codec context\n");
        return false;
    }

    // Copy codec parameters from input stream to output codec context
    if (avcodec_parameters_to_context(state->codec_ctx, stream->codecpar) < 0) {
        fprintf(stderr, "Failed to copy codec parameters to decoder context\n");
        return false;
    }

    // Initialize the decoder
    if (avcodec_open2(state->codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return false;
    }

    return true;
}

//allocation and configuration of the translator
bool init_translator( AudioPlayerState *state )
{
    if (state->should_exit) return false;

    SwrContext *swr_ctx = swr_alloc();

    if(!swr_ctx){
        fprintf(stderr, "Could not allocate resampler context\n");
        return false;
    }   

    av_opt_set_chlayout(swr_ctx, "in_chlayout", &state->codec_ctx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", state->codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", state->codec_ctx->sample_fmt, 0);



    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_opt_set_chlayout(swr_ctx, "out_chlayout",    &out_ch_layout, 0);
    av_opt_set_int(swr_ctx,      "out_sample_rate",  AUDIO_SAMPLE_RATE, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0); // Packed Float


    if (swr_init(swr_ctx) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return false;
    }


    state->swr_ctx = swr_ctx;
    return true;
    //configure the translator to convert the audio to the format expected by raylib
}


// Decode a single audio frame and push it to the ring buffer
// may want to spawn pthread in main invoking this function in a loop to keep decoding while the player is playing


void write_to_buffer( RingBuffer *rb, const float * data, int num_samples )
{
    pthread_mutex_lock(&rb->buffer_mutex);

    // Write data to the ring buffer
    for (int i = 0; i < num_samples; i++) 
    {
        rb->buffer[rb->write_ptr] = data[i];

        //we need to check if the consumer has caught up to the producer, if so we will overwrite the oldest data in the buffer     

        if (rb->write_ptr == rb->read_ptr) 
        {   
            rb->read_ptr = (rb->read_ptr + 1) % (BUFFER_SIZE);
        }
    }

    pthread_mutex_unlock(&rb->buffer_mutex);

}

void dump_ring_buffer( RingBuffer *rb )
{
    for(int i = 0; i < BUFFER_SIZE; i++)
    {
        if (!rb->buffer[i]) break;
        printf ("%f ", rb->buffer[i]);
    }    
    printf("\n");
}

void decode_audio_frame( AudioPlayerState *state, RingBuffer *rb ) {

    // Allocate packet and frame
    
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (av_read_frame(state->format_ctx, packet) >= 0) 
    {
        if (packet->stream_index == state->audio_stream_index) 
        {        
            
            if(avcodec_send_packet(state->codec_ctx, packet) >= 0) 
            {

                 while (avcodec_receive_frame(state->codec_ctx, frame) >= 0) 
                {
                    
                    
                    int64_t delay =  swr_get_delay(state->swr_ctx, frame->sample_rate);
                    int out_count = av_rescale_rnd(delay + frame->nb_samples, AUDIO_SAMPLE_RATE, 
                                                   frame->sample_rate, AV_ROUND_UP);
                    uint8_t *out_buffer = NULL;
                    int out_linesize;
                    av_samples_alloc(&out_buffer, &out_linesize, AUDIO_CHANNELS, out_count, AV_SAMPLE_FMT_FLT, 0);
                    int converted_samples = swr_convert(state->swr_ctx, &out_buffer, out_count, 
                                                       (const uint8_t **)frame->data, frame->nb_samples);
                    if (converted_samples > 0)
                    {
                        // Write the converted samples to the ring buffer
                        write_to_buffer(rb, (float *)out_buffer, converted_samples * AUDIO_CHANNELS);
                    }

                    
                    //debug - want to see what is in ring buffer

                    dump_ring_buffer(&rb);

                    av_freep(&out_buffer);

                    av_frame_unref(frame);

                }

            }
        
        }

    }

    int flush_samples = swr_convert(state->swr_ctx, NULL, 0, NULL, 0);


    av_packet_unref(packet);
    av_frame_free(&frame);
    av_packet_free(&packet);
}

void close_decoder(AudioPlayerState *state) {
    if (state->codec_ctx) avcodec_free_context(&state->codec_ctx);
    if (state->format_ctx) avformat_close_input(&state->format_ctx);
    if (state->swr_ctx) swr_free(&state->swr_ctx);
}