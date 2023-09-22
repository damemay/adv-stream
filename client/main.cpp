#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <cpr/cpr.h>
#include <iostream>
#include <string.h>
#include <string>

#define IMAGE           0
#define TEXT            1
#define HANDLES         2
#define MAX_BUFFER      5120
#define URL             "http://192.168.1.26:8080"

typedef struct memory {
    char ptr[MAX_BUFFER];
    size_t size;
} memory;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* canvas = NULL;
SDL_Event event;
int raw_pitch = 0;
void* raw_pixels = NULL;
int frame_pos = 0;
int foo = 1;
int run = 1;
int line = 0;
std::vector<cpr::AsyncWrapper<cpr::Response, true>> response;

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

static size_t read_line(void* ptr, size_t size, size_t nmemb, void* stream) {
    memory* mem = (memory*)stream;
    long nbytes = size*nmemb;
    memcpy(&(mem->ptr[mem->size]), ptr, nbytes);
    mem->size += nmemb;
    puts(mem->ptr);
    return nbytes;
}

static size_t read_img(void* ptr, size_t size, size_t nmemb, void* stream) {
    memory* mem = (memory*)stream;
    long nbytes = size*nmemb;
    memcpy(&(mem->ptr[mem->size]), ptr, nbytes);
    mem->size += nbytes;
    return nbytes;
}

static int show_new_frame(const char* ptr, size_t size) {
    SDL_RWops* rwops = SDL_RWFromConstMem(ptr, size);
    if(!rwops) {
        puts("rwops");
        return 1;
    }

    SDL_Surface* tmp = NULL;
    tmp = IMG_LoadJPG_RW(rwops);
    if(!tmp) {
        puts("loadjpg_rw");
        return 1;
    }
    SDL_Surface* image = NULL;
    image = SDL_ConvertSurfaceFormat(tmp, SDL_PIXELFORMAT_RGBA32, 0);
    if(!image) {
        puts("convertsurfaceformat");
        return 1;
    }
    copy_buffer(&canvas, &raw_pitch, &raw_pixels, image->pixels);
    SDL_FreeSurface(tmp);
    SDL_FreeSurface(image);
    SDL_FreeRW(rwops);
    SDL_RenderCopy(renderer, canvas, NULL, NULL);
    SDL_RenderPresent(renderer);
    return 0;
}

bool writeCallback(std::string data, intptr_t userdata) {
    size_t size = data.size();
    memory* mem = (memory*)userdata;

    const char* pos = strstr(data.c_str(), "Content-Length: ");
    if(pos) mem->size = atoi(pos+16);

    const char* bptr = data.c_str();
    for(size_t i=0; i<data.size(); ++i) {
        if(foo) {
            if(bptr[i] == '\n' && bptr[i-1] == '\r' && 
                bptr[i-2] == '\n' && bptr[i-3] == '\r') foo = 0;
        } else {
            if(frame_pos < MAX_BUFFER) mem->ptr[frame_pos] = bptr[i];
            frame_pos++;
            if(frame_pos >= mem->size) {
                frame_pos = 0;
                show_new_frame(mem->ptr, mem->size);
                foo = 1;
            }
        }
    }
    return true;
}

static nullptr_t parse_line(std::string line, memory* text) {
    if(line.find("@src ") == std::string::npos) return nullptr;
    std::string uri = line.substr(5);
    auto url = cpr::Url{URL + uri};
    if(uri.find(".jpg") != std::string::npos) {
        auto r = cpr::Get(url);
        show_new_frame(r.text.c_str(), r.text.size());
    } else {
        if(!response.empty()) {
            if(response.at(0).wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout) {
                (response.at(0).Cancel() == cpr::CancellationResult::success);
                response.clear();
                frame_pos = 0;
                foo = 1;
            }
        }
        response = MultiGetAsync(std::tuple{
            url, cpr::WriteCallback{writeCallback, (intptr_t)text}
        });
    }
    return nullptr;
}

static std::string get_line(int* line) {
    auto url = cpr::Url{URL "/get-line"};
    auto param = cpr::Parameters{{"line", std::to_string(*line)}};
    auto r = cpr::Get(url, param);
    if(r.status_code != 200) {
        *line--;
        return {};
    }
    std::cout << r.text << std::endl;
    return r.text;
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

    memory image = {0};

    parse_line(get_line(&line), &image);

    while(run) {
        while(SDL_PollEvent(&event) != 0) {
            if(event.type == SDL_QUIT) run = 0;
            if(event.type == SDL_KEYUP && event.key.repeat == 0)
                if(event.key.keysym.sym == SDLK_RETURN) ++line, parse_line(get_line(&line), &image);
        }
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return EXIT_SUCCESS;
}
