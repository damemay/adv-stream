#include "mongoose.h"
#include "data.h"

char** script = NULL;
size_t lines = 0;

static void free_file_in_lines(char** file, const size_t line_count) {
    for(size_t i=0; i<line_count; ++i) free(file[i]);
    free(file);
}

static char** read_file_in_lines(const char* path, size_t* line_count) {
    FILE* file = fopen(path, "rb");
    if(!file) perror("file"), exit(EXIT_FAILURE);

    fseek(file, 0L, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char** code = malloc(size+1);
    if(!code)
        fclose(file), exit(1);

    char line[1024];
    size_t i = 0;
    while(fgets(line, sizeof(line), file) != NULL) {
        char* n;
        if((n = strchr(line, '\n')) != NULL)
            line[n-line] = '\0';
        code[i] = strdup(line);
        ++i;
    }

    if(feof(file)) fclose(file);

    *line_count = i;
    return code;
}


#define stream_print() \
    c->data[0] = 'S'; \
    mg_printf( \
        c, "%s", \
        "HTTP/1.0 200 OK\r\n" \
        "Cache-Control: no-cache\r\n" \
        "Pragma: no-cache\r\nExpires: Thu, 01 Dec 1994 16:00:00 GMT\r\n" \
        "Content-Type: multipart/x-mixed-replace; boundary=--foo\r\n\r\n");

static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if(ev == MG_EV_OPEN) {
        c->fn_data = malloc(1000);
    } else if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        if (mg_http_match_uri(hm, "/api/video1")) {
            if(c->fn_data) strcpy(c->fn_data, "video1");
            stream_print();
        } else if(mg_http_match_uri(hm, "/red")) {
            if(c->fn_data) strcpy(c->fn_data, "red");
            stream_print();
        } else if(mg_http_match_uri(hm, "/get-line")) {
            if(!script || !lines) mg_http_reply(c, 503, NULL, "Script or lines were not initialized!");
            struct mg_str caps[2];
            if(mg_match(hm->query, mg_str("line=*"), caps)) {
                int line = atoi(caps[0].ptr);
                if(line<lines) mg_http_reply(c, 200, NULL, "%s", script[atoi(caps[0].ptr)]);
                else mg_http_reply(c, 400, NULL, "Line exceedes line count!");
            }
            mg_http_reply(c, 400, NULL, "Line not found");
        } else {
            struct mg_http_serve_opts opts = {.root_dir = "web_root"};
            mg_http_serve_dir(c, ev_data, &opts);
        }
    } else if(ev == MG_EV_CLOSE) {
        if(c->fn_data) free(c->fn_data);
        c->fn_data = NULL;
    }
    (void) fn_data;
}

#define broadcast_work() \
    size_t nfiles = sizeof(files) / sizeof(files[0]); \
    static size_t i; \
    const char *path = files[i++ % nfiles]; \
    size_t size = 0; \
    char *data = mg_file_read(&mg_fs_posix, path, &size); \
    if (data == NULL || size == 0) continue; \
    mg_printf(c, \
            "--foo\r\nContent-Type: image/jpeg\r\n" \
            "Content-Length: %lu\r\n\r\n", \
            (unsigned long) size); \
    mg_send(c, data, size); \
    mg_send(c, "\r\n", 2); \
    free(data);

static void broadcast_mjpeg_frame(struct mg_mgr *mgr) {
    struct mg_connection *c;
    for (c = mgr->conns; c != NULL; c = c->next) {
        if (c->data[0] != 'S') continue;         // Skip non-stream connections
        if(strcmp(c->fn_data, "video1")==0) {
            const char *files[] = {"images/1.jpg", "images/2.jpg", "images/3.jpg", "images/4.jpg", "images/5.jpg", "images/6.jpg"};
            broadcast_work();
        } else if(strcmp(c->fn_data, "red")==0) {
            const char *files[] = {RED_CUBE};
            broadcast_work();
        }
    }
}

static void timer_callback(void *arg) {
    broadcast_mjpeg_frame(arg);
}

int main(void) {
    struct mg_mgr mgr;

    script = read_file_in_lines("test.adv", &lines);

    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "http://0.0.0.0:8080", cb, NULL);
    mg_timer_add(&mgr, 1000/60, MG_TIMER_REPEAT, timer_callback, &mgr);
    for (;;) mg_mgr_poll(&mgr, 50);
    mg_mgr_free(&mgr);

    free_file_in_lines(script, lines);

    return 0;
}
