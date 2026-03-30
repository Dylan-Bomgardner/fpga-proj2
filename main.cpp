#include <fcntl.h>
#include <sys/mman.h>
#include <cstdint>
#include <iostream>
#include <time.h>
#include <vector>
#include <mpg123.h>
#include <string>


#define HPS_BASE 0xC0000000
#define LWHPS_BASE 0xFF200000

#define LEFT_FIFO_BASE  HPS_BASE + 0x00000000
#define RIGHT_FIFO_BASE HPS_BASE + 0x00010000

#define PIO0_BASE LWHPS_BASE + 0x00000000
#define PIO1_BASE LWHPS_BASE + 0x00010000

#define HPS_SIZE 0x00020000
static constexpr long   SAMPLE_RATE      = 48000;
static constexpr long   SAMPLE_PERIOD_NS = 1000000000L / SAMPLE_RATE;
static mpg123_handle* mh;

std::vector<uint32_t> pcm_data;
bool fifo_full(volatile uint32_t* hps_base) {
    const uint32_t csr_offset = 0x1000;
    const uint32_t status_offset = 4;
    // std::cout << "getting almost full" << std::endl;
    uint32_t almost_full = *(uint32_t*)((uint8_t*)hps_base + csr_offset + status_offset);
    almost_full = almost_full & 0x1;
    // if (almost_full) 
        // std::cout << "Almost Full: " << almost_full << std::endl;
    return (almost_full == 1);
}

void change_song(volatile uint32_t* ptr, uint32_t data) {
    // write song number to pio1
    // std::cout << "ptr: " << (void*)ptr << std::endl;
    // std::cout << "before change song" << std::endl;
    const uint32_t pio1_offset = 0x00010000;
    std::cout << std::hex << (void*)ptr + pio1_offset << std::endl;
    *(uint32_t*)((uint8_t*)ptr + pio1_offset) = data;
}

void write_fifo(volatile uint32_t* ptr, uint32_t data) {
    *ptr = data;
}


bool stop_playing(volatile uint32_t* ptr) {
    // if (*ptr != 3)
        // std::cout << "stop playing: " << std::hex << *ptr << std::endl;
    return *ptr;
}

bool should_play_next(uint32_t val) {
    bool play_next = ~val & 1;
    // if (play_next) std::cout << "requested play next" << std::endl;
    return play_next;
}

bool should_play_prev(uint32_t val) {
    bool play_prev = ~val & 2;
    // if (play_prev) std::cout << "requested play prev" << std::endl;
    return play_prev;
}

int load_song(int track_number) {
    std::string file_name = "/" + std::to_string(track_number);
    file_name = file_name + ".mp3";
    std::cout << "loading: " << file_name << std::endl;

    if (mpg123_open(mh, file_name.c_str()) != MPG123_OK) {
        fprintf(stderr, "Cannot open: %s\n", file_name.c_str());
        return -1;
    }

    std::cout << "song opened" << std::endl;

    mpg123_format_none(mh);
    mpg123_format(mh, 48000, MPG123_STEREO, MPG123_ENC_SIGNED_24);

    size_t buffer_size = mpg123_outblock(mh);
    std::vector<uint8_t> buf(buffer_size);

    size_t bytes_decoded;
    while (mpg123_read(mh, buf.data(), buffer_size, &bytes_decoded) == MPG123_OK) {
        // each sample is 3 bytes
        size_t num_samples = bytes_decoded / 3;  
        for (size_t i = 0; i < num_samples; i++) {
            // reassemble 3 bytes into a 24-bit value in a uint32_t
            uint32_t sample = (uint32_t)buf[i*3 + 0]
                            | (uint32_t)buf[i*3 + 1] << 8
                            | (uint32_t)buf[i*3 + 2] << 16;
            pcm_data.push_back(sample);
            
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    // open memory device
    if (argc < 2) {
        std::cout << "Usage ./program <num tracks>" << std::endl;
        return -1;
    }
    int num_tracks = atoi(argv[1]);
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("cannot access /dev/mem");
        return -1;
    }


    volatile uint32_t* hps = (uint32_t*) mmap(nullptr, (size_t)HPS_SIZE, 
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, LEFT_FIFO_BASE); 
    if (hps == MAP_FAILED) {
        perror("error with mmap hps");
        return -1;
    }

    volatile uint32_t* lwhps = (uint32_t*) mmap(nullptr, (size_t)HPS_SIZE, 
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, PIO0_BASE);

    if (lwhps == MAP_FAILED) {
        perror("error with mmap lwhps");
        return -1;
    }

    
    mpg123_init();
    int err;
    mh = mpg123_new(nullptr, &err);
    if (!mh) { fprintf(stderr, "mpg123_new failed\n"); return 1; }

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    int curr_song = 0;
    while(1) {
        pcm_data.clear();
        mpg123_close(mh);

        change_song(lwhps, curr_song);
        if (load_song(curr_song) < 0) break;
        std::cout << "loaded song" << std::endl;
        // std::cout << "pcm data size: " << pcm_data.size() << std::endl;
        for(size_t i = 0; i + 1 < pcm_data.size(); i+=2) {
            // Check if a song change pio has been enabled
            uint32_t val;
            if ((val = stop_playing(lwhps)) == 0) {
                if (should_play_next(val)) {
                    curr_song = (curr_song + 1) % num_tracks;
                } else if (should_play_prev(val)) {
                    curr_song = (curr_song - 1) % num_tracks;
                }
                std::cout << "changing song" << std::endl;
                
                // change song and restart
                break;
            }
            // std::cout << "loading left and right" << std::endl;
            const uint32_t left = pcm_data[i];
            const uint32_t right = pcm_data[i + 1];
            //  std::cout << "left: " << std::hex << pcm_data[i] << " right: " << pcm_data[i + 1] << std::dec << std::endl;

            // only write once both fifos are not full
            while (fifo_full(hps));
            const uint32_t right_fifo_offset = 0x00010000;
            while (fifo_full((uint32_t*)((uint8_t*)hps + right_fifo_offset)));

            write_fifo(hps, left);
            write_fifo((uint32_t*)((uint8_t*)hps + right_fifo_offset), right);
            if (i + 3 >= pcm_data.size()) {
                curr_song = (curr_song + 1) % num_tracks;
            }
           
        }

        // // finished song play next
        // curr_song = (curr_song++) % num_tracks;


    }

    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    std::cout << "ending program" << std::endl;

    return 0;
}