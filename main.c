#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "pcg_basic.h"
#include "pcg_basic.c"
#include "fenster.h"

#define log printf

typedef struct {
    size_t index;
    uint8_t pixel[4];
} Pixel;

typedef enum {
    Ordering_LessThan,
    Ordering_Equal,
    Ordering_GreaterThan,
} Ordering;

void print_pixels(Pixel* pixels, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%zu ", pixels[i].index);
    }
    printf("length: %zu\n", len);
}

void merge(Pixel* a, size_t len_a, Pixel* b, size_t len_b, Pixel* out) {
    size_t ai = 0;
    size_t bi = 0;
    size_t oi = 0;
    while (ai < len_a && bi < len_b) {
        if (a[ai].index <= b[bi].index) {
            out[oi] = a[ai];
            ai += 1;
            oi += 1;
        } else {
            out[oi] = b[bi];
            bi += 1;
            oi += 1;
        }
    }
    assert(bi == len_b || ai == len_a);
    while (ai < len_a) {
        out[oi] = a[ai];
        ai += 1;
        oi += 1;
    }
    while (bi < len_b) {
        out[oi] = b[bi];
        bi += 1;
        oi += 1;
    }
    assert(ai == len_a && bi == len_b);
}

static int step = 0;
static int64_t lasttime = 0;
void mergesort_impl(struct fenster* f, Pixel* base, size_t base_len, Pixel* pixels, size_t len, Pixel* tmp) {
    if (len <= 1) {
        return;
    }
    else {
        mergesort_impl(f, base, base_len, pixels+0, len/2, tmp);
        mergesort_impl(f, base, base_len, pixels+len/2, len-len/2, tmp);
        merge(pixels+0, len/2, pixels+len/2, len-len/2, tmp);
        size_t last_it = 0;
        for (size_t it = 0; it < len; it++) {
            pixels[it] = tmp[it];
            if (len > base_len / 512) {
                if (step == 0) {
                    for(size_t i = pixels-base+last_it; i < pixels-base+it; i++) {
                        uint32_t pixel = base[i].pixel[0] << 16 | base[i].pixel[1] <<8 | base[i].pixel[2];
                        f->buf[i] = pixel;
                    }
                    last_it = it;
                    if (!lasttime) {
                        lasttime = fenster_time();
                    }
                    int64_t duration = lasttime - fenster_time();
                    fenster_sleep(16-duration);
                    lasttime = fenster_time();
                    fenster_loop(f);
                }
                step += 1;
                step %= base_len / 500;
            }
        }
        
    }
}

void mergesort(struct fenster* f, Pixel* pixels, size_t len) {
    Pixel* tmp = malloc(sizeof(Pixel)*len);
    mergesort_impl(f, pixels, len, pixels, len, tmp);
    free(tmp);
}

void shuffle_pixels(Pixel* pixels, size_t len) {
    for (size_t i = len-1; i >= 1; i--) {
        uint32_t j = pcg32_boundedrand(i+1);
        Pixel tmp = pixels[i];
        pixels[i] = pixels[j];
        pixels[j] = tmp;
    }
}

Ordering cmp_pixel_by_index(Pixel* a, Pixel* b) {
    if (a->index < b->index) {
        return Ordering_LessThan;
    }
    else if (a->index == b->index) {
        return Ordering_Equal;
    }
    else {
        return Ordering_GreaterThan;
    }
}

Ordering cmp_pixel_by_value(Pixel* a, Pixel* b) {
    
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: imsort.exe <filename>\n");
        return 0;
    }
    uint32_t width, height, unused;
    uint32_t channels = 4;
    uint8_t* image = stbi_load(argv[1], (int*)&width, (int*)&height, (int*)&unused, channels);
    Pixel* pixels = malloc(sizeof(Pixel) * width * height);
    assert(pixels && "Buy more RAM");
    for (size_t i = 0; i < width * height; i++) {
        pixels[i] =  (Pixel){.index=i, .pixel={
            image[i*channels], image[i*channels+1], image[i*channels+2], image[i*channels+3]
        }};
    }

    uint32_t* fenster_buffer = malloc(width*height*sizeof(uint32_t));
    assert(fenster_buffer && "Buy more RAM");
    struct fenster f = {.title = "Sort", .width=width, .height=height, .buf=fenster_buffer};
    fenster_open(&f);
    while (fenster_loop(&f) == 0) {
        shuffle_pixels(pixels, width*height);
        for(int i = 0; i < f.width; i++) {
            for (int j = 0; j < f.height; j++) {
                uint32_t pixel = pixels[j*f.width+i].pixel[0] << 16 | pixels[j*f.width+i].pixel[1] <<8 | pixels[j*f.width+i].pixel[2];
                fenster_pixel(&f, i, j) = pixel;
            } 
        }
        mergesort(&f, pixels, width*height);
        for(int i = 0; i < f.width; i++) {
            for (int j = 0; j < f.height; j++) {
                uint32_t pixel = pixels[j*f.width+i].pixel[0] << 16 | pixels[j*f.width+i].pixel[1] <<8 | pixels[j*f.width+i].pixel[2];
                fenster_pixel(&f, i, j) = pixel;
            } 
        }
        fenster_sleep(100);
    }
    fenster_close(&f);
    /*
    if(pixels) free(pixels);
    if(fenster_buffer) free(fenster_buffer);
    if(image) stbi_image_free(image);
    */
    return 0;
}
