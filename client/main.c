#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>

#define MAX_BUFFER 5120
typedef struct memory {
    char ptr[MAX_BUFFER];
    size_t size;
} memory;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* canvas = NULL;
int raw_pitch = 0;
void* raw_pixels = NULL;
int frame_pos = 0;
int foo = 1;

static void lock(SDL_Texture** texture, int* raw_pitch, void** raw_pixels) {
    if(!*raw_pixels) 
        if(SDL_LockTexture(*texture, NULL, raw_pixels, raw_pitch) != 0)
            exit(1);
}

static void unlock(SDL_Texture** texture, int* raw_pitch, void** raw_pixels) {
    if(*raw_pixels) {
        SDL_UnlockTexture(*texture);
        *raw_pitch = 0;
        *raw_pixels = NULL;
    }
}

static void copy_pixels(int raw_pitch, void* raw_pixels, void* pixels) {
    if(raw_pixels)
        memcpy(raw_pixels, pixels, raw_pitch*512);
}

static void copy_buffer(SDL_Texture** texture,
        int* raw_pitch, void** raw_pixels, void* buffer) {
    lock(texture, raw_pitch, raw_pixels);
    copy_pixels(*raw_pitch, *raw_pixels, buffer);
    unlock(texture, raw_pitch, raw_pixels);
}


static size_t write(void* ptr, size_t size, size_t nmemb, void* stream) {
    size_t written;

    memory* mem = (memory*)stream;

    long nbytes = size*nmemb;
    printf("received %d bytes\n", nbytes);

    char* pos = strstr(ptr, "Content-Length: ");
    if(pos) {
        mem->size = atoi(pos+16);
        printf("size: %d\n", mem->size);
    }

    unsigned char* bptr = (unsigned char*)ptr;
    for(size_t i=0; i<nbytes; ++i) {
        unsigned char b = bptr[i];
        if(foo) {
            unsigned char c = bptr[i-1], d = bptr[i-2], e = bptr[i-3];
            if(b == '\n' && e == '\r' && d == '\n' && e == '\r')
                foo = 0;
        } else {
            if(frame_pos < MAX_BUFFER) mem->ptr[frame_pos] = b;
            frame_pos++;
            if(frame_pos >= mem->size) {
                frame_pos = 0;
                SDL_RWops* rwops = SDL_RWFromConstMem(mem->ptr, mem->size);
                if(!rwops) puts("rwops"), exit(1);
                SDL_Surface* tmp = NULL;
                tmp = IMG_LoadJPG_RW(rwops);
                if(!tmp) puts("loadjpg_rw"), exit(1);
                SDL_Surface* image = NULL;
                image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
                if(!image) puts("convertsurfaceformat"), exit(1);
                copy_buffer(&canvas, &raw_pitch, &raw_pixels, image->pixels);
                SDL_FreeSurface(tmp);
                SDL_FreeSurface(image);
                SDL_FreeRW(rwops);
                SDL_RenderCopy(renderer, canvas, NULL, NULL);
                SDL_RenderPresent(renderer);
                foo = 1;
            }
        }
    }

    return nbytes;
}

int main(int argc, char* argv[]) {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) return EXIT_FAILURE;
    atexit(SDL_Quit);
    if(!(window = SDL_CreateWindow("example",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            512, 512, SDL_WINDOW_SHOWN))) return EXIT_FAILURE;
    if(!(renderer = SDL_CreateRenderer(window, -1,
            SDL_RENDERER_ACCELERATED))) return EXIT_FAILURE;
    if(!(canvas = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
            512, 512))) return EXIT_FAILURE;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl;
    CURLcode curl_res;
    if((curl = curl_easy_init()) == NULL) return EXIT_FAILURE;

    // curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.26:8080/0001.jpg");
    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.26:8080/red");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
    memory test = {0};
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &test);

    if((curl_res = curl_easy_perform(curl)) != CURLE_OK) {
        puts(curl_easy_strerror(curl_res));
        return EXIT_FAILURE;
    }

    // SDL_RWops* rwops = SDL_RWFromConstMem(test.ptr, test.size);
    // if(!rwops) puts("rwops"), exit(1);
    // SDL_Surface* tmp = NULL;
    // tmp = IMG_LoadJPG_RW(rwops);
    // if(!tmp) puts("loadjpg_rw"), exit(1);
    // SDL_Surface* image = NULL;
    // image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
    // if(!image) puts("convertsurfaceformat"), exit(1);
    // copy_buffer(&canvas, &raw_pitch, &raw_pixels, image->pixels);
    // SDL_FreeSurface(tmp);
    // SDL_FreeSurface(image);
    // SDL_FreeRW(rwops);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // while(1) {
    //     SDL_RenderCopy(renderer, canvas, NULL, NULL);
    //     SDL_RenderPresent(renderer);
    // }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return EXIT_SUCCESS;
}
