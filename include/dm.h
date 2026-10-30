#ifndef DM_H
#define DM_H

#include <curl/curl.h>
#include "util.h"

#define VERSION_MAJOR   1
#define VERSION_MINOR   0

#define ERR_BUF_SIZE    256 
#define INPUT_BUF_SIZE  1024 
#define MIN_ARGC        2
#define MAX_PARALLEL    10
#define URL_BASE_CAP    10
#define URL_GROWTH_FACT 2
#define ARG_FMT         ":vhd:f:"

#define BYTES_PER_GIB      1073741824ULL
#define BYTES_PER_MIB      1048576ULL
#define BYTES_PER_KIB      1024ULL

#define FOLLOWLOCATION_FLAG   1L

/* TODO: Implement file-reading functionality. */
enum input_method {
        INPUT_CLI,
        INPUT_FILE
};

enum download_mode {
        DOWNLOAD_ASYNC,
        DOWNLOAD_SYNC
};

struct dm_context  {
        int input_method;
        char *input_filepath;
        char input_buf[INPUT_BUF_SIZE];

        char *dirpath;
};

struct dm_download {
        CURL *curl;
        FILE *file;

        size_t url_count;
        char **url;
        char **filename;
        char **filepath;

        const char *current_filename;
};

enum suffix_idx {
        GB_SUFFIX_IDX,
        MB_SUFFIX_IDX,
        KB_SUFFIX_IDX,
        BYTES_SUFFIX_IDX
};

static const char *const size_suffix[] = {
        "GiB",
        "MiB",
        "KiB",
        "bytes"
};

static const char *const speed_suffix[] = {
        "GiB/s",
        "MiB/s",
        "KiB/s",
        "bytes/s"
};

static const char dlstr[] = "#########################";
static size_t dlstr_len = ARR_LEN(dlstr) - 1;

size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp);

int dm_context_init(struct dm_context *dm_ctx, int input_method);
void dm_context_cleanup(struct dm_context *dm_ctx);

int dm_download_init(struct dm_download *dl, CURL *curl);
void dm_download_cleanup(struct dm_context *dm_ctx, struct dm_download *dl);

int dm_download(struct dm_context *dm_ctx, struct dm_download *dl, int download_mode);
int dm_download_sync(struct dm_download *dl, const char *path, const char *url);

void dm_parse_args(struct dm_context *dm_ctx, int argc, char *argv[], const char *arg_fmt);
int dm_validate_args(struct dm_context *dm_ctx, int url_start_idx, int argc);

int dm_input_cli(struct dm_download *dl, int url_start_idx, int argc, char *argv[]);
int dm_input_file(struct dm_context *dm_ctx, struct dm_download *dl, FILE *input_file);

int dm_conv_size(double *size, const char **suffix);
int dm_conv_speed(double *speed, const char **suffix);

void dm_log_version();
void dm_log_helpstr();
void dm_log_download_progress(struct dm_download *dl);

#endif
