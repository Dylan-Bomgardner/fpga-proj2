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

void write_fifo(volatile uint32_t* ptr, uint32_t data) {
    *ptr = data;
}


bool stop_playing(volatile uint32_t* ptr) {
    return *ptr;
}

bool should_play_next(uint32_t val) {
    return val & 0b10;
}

bool should_play_prev(uint32_t val) {
    return val & 0b1;
}

int load_song(int track_number) {
    std::string file_name = "/home/dylan/Downloads/" + std::to_string(track_number);
    file_name = file_name + ".mp3";

    if (mpg123_open(mh, file_name.c_str()) != MPG123_OK) {
        fprintf(stderr, "Cannot open: %s\n", file_name.c_str());
        return -1;
    }

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
    // int fd = open("/dev/mem", O_RDWR | O_SYNC);
    // if (fd < 0) {
    //     perror("cannot access /dev/mem");
    //     return -1;
    // }

    // volatile uint32_t* hps = (uint32_t*) mmap(nullptr, (size_t)HPS_SIZE, 
    //     PROT_READ | PROT_WRITE, MAP_SHARED, fd, LEFT_FIFO_BASE); 
    // if (hps == nullptr) {
    //     perror("error with mmap hps");
    //     return -1;
    // }

    // volatile uint32_t* lwhps = (uint32_t*) mmap(nullptr, (size_t)HPS_SIZE, 
    //     PROT_READ | PROT_WRITE, MAP_SHARED, fd, PIO0_BASE);

    // if (lwhps == nullptr) {
    //     perror("error with mmap lwhps");
    //     return -1;
    // }

    
    mpg123_init();
    int err;
    mh = mpg123_new(nullptr, &err);
    if (!mh) { fprintf(stderr, "mpg123_new failed\n"); return 1; }

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    int curr_song = 0;
    while(1) {
        mpg123_close(mh);
        if (load_song(curr_song) < 0) break;

        for(size_t i = 0; i + 1 < pcm_data.size(); i+=2) {
            // get status
            // if (uint32_t val = stop_playing(lwhps)) {
            //     if (should_play_next(val)) {
            //         curr_song = (curr_song++) % num_tracks;
            //     } else if (should_play_prev) {
            //         curr_song = (curr_song--) % num_tracks;
            //     }
            //     std::cout << "changing song" << std::endl;
                
            //     break;
            // }
            const uint32_t left = pcm_data[i];
            const uint32_t right = pcm_data[i + 1];
            // write_fifo(hps, left);
            // write_fifo(hps + 0x00010000, right);
            std::cout << "left: " << std::hex << pcm_data[i] << " right: " << pcm_data[i + 1] << std::dec << std::endl;

            next.tv_nsec += SAMPLE_PERIOD_NS;
            if (next.tv_nsec >= 1000000000L) {
                next.tv_nsec -= 1000000000L;
                next.tv_sec++;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, nullptr);

        }

        // finished song play next
        curr_song = (curr_song++) % num_tracks;


    }

    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();

    std::cout << "ending program" << std::endl;

    return 0;
}