#include "raylib.h"
#include "decoder.h"
#include <stdio.h>



static RingBuffer rb = 
{
    .buffer = {0},
    .write_ptr = 0, 
    .read_ptr = 0, 
    .buffer_mutex = PTHREAD_MUTEX_INITIALIZER 
};



//raylib audio reader from rb

void raylib_audio_callback(void *buffer, unsigned int frames) 
{
    float *out = (float *)buffer;
    //Samples = frames * AUDIO_CHANNELS (stereo)
    unsigned int samples_to_write = frames * AUDIO_CHANNELS;

    //critical section
    pthread_mutex_lock(&rb.buffer_mutex);

    for (unsigned int i = 0; i < samples_to_write; i++) {
        if (rb.read_ptr == rb.write_ptr) 
        {
            // Buffer is empty, fill with silence
            out[i] = 0.0f;
        } 
        else 
        {
            out[i] = rb.buffer[rb.read_ptr];
            //increment read pointer, mod is to wrap around the buffer size
            rb.read_ptr = (rb.read_ptr + 1) % (AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * SECONDS_BUFFER);
        }
    }
    //end critical section

    pthread_mutex_unlock(&rb.buffer_mutex);

    // This function will be called by Raylib to fill the audio buffer
    // You need to read from your ring buffer and fill the provided buffer
    // with the appropriate number of frames (samples).
}







int main(int argc, char **argv) {

    // if (argc < 2) {
    //     printf("Usage: %s <path_to_audio_file>\n", argv[0]);
    //     return 1;
    // }


    // Initialize Raylib Window
    const int screenWidth = 800;
    const int screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "Terminal Music Player");
    SetTargetFPS(60);

    // Initialize Raylib Audio device
    InitAudioDevice();

    AudioPlayerState player = {0};
    AudioStream stream = LoadAudioStream(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, 32); // 32-bit float

    //set the audio stream callback to custom function
    SetAudioStreamCallback(stream, raylib_audio_callback);


    // close the Device/window if the decoder fails to initialize
    if (!init_decoder(&player, argv[1]) || !init_translator(&player)) {
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }


    

    player.is_playing = true;


    // GUI loop

    while (!WindowShouldClose()) {

        // --- Input Handling ---
        if (IsKeyPressed(KEY_SPACE)) {
            player.is_playing = !player.is_playing;
        }

        //--- Logic / Decoding ---
        if (player.is_playing) {
            decode_audio_frame(&player, &rb);
            PlayAudioStream(stream);
        }

        // --- Render UI ---
        BeginDrawing();

            ClearBackground(BLACK);

            DrawText("TERMINAL MUSIC PLAYER", 20, 20, 20, GREEN);
            DrawText(TextFormat("Playing: %s", player.filepath), 20, 60, 16, LIGHTGRAY);
            
            if (player.is_playing) {
                DrawText("[PLAYING] Press SPACE to Pause", 20, 120, 16, LIME);
            } else {
                DrawText("[PAUSED] Press SPACE to Play", 20, 120, 16, MAROON);
            }
            
        EndDrawing();
    }

    // Cleanup
    close_decoder(&player);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}