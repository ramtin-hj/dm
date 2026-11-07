#ifndef DM_H
#define DM_H
#include <stdbool.h>
#include <curl/curl.h>
#include <parg/parg.h>
#include "util.h"

/* DM constants */
#define VERSION_MAJOR   1
#define VERSION_MINOR   0
#define MAX_PARALLEL    10
#define MIN_ARGC        2
#define ARG_FMT         ":vhd:f:"

/* Buffer sizes */
#define ERR_BUF_SIZE    256 
#define INPUT_BUF_SIZE  1024 
#define TIME_BUF_SIZE   16 

/* Dynamic array constants */
#define DYN_ARR_BASE_CAP        10
#define DYN_ARR_GROWTH_FACT     2

/* Size units and conversions */
#define BYTES_PER_GIB           1073741824ULL
#define BYTES_PER_MIB           1048576ULL
#define BYTES_PER_KIB           1024ULL
#define BITS_PER_GIGABIT        1000000000ULL
#define BITS_PER_MEGABIT        1000000ULL
#define BITS_PER_KILOBIT        1000ULL
#define BITS_PER_BYTE           8ULL

/* Time units and conversions */
#define SECONDS_PER_DAY         86400ULL
#define SECONDS_PER_HOUR        3600ULL
#define SECONDS_PER_MINUTE      60ULL
#define MINUTES_PER_HOUR        60ULL

/* Libcurl flags */
#define PROGRESS_BAR_FLAG       1L
#define FOLLOWLOCATION_FLAG     1L
#define LOW_SPEED_LIMIT_FLAG    1L
#define LOW_SPEED_TIME_FLAG     30L

/* File modes */
#define FILE_MODE_READ_BINARY           "rb"
#define FILE_MODE_WRITE_BINARY          "wb"
#define FILE_MODE_APPEND_BINARY         "ab"
#define FILE_MODE_READ                  "r"
#define FILE_MODE_WRITE                 "w"
#define FILE_MODE_APPEND                "a"
#define FILE_MODE_READ_WRITE            "r+"
#define FILE_MODE_READ_WRITE_TRUNC      "w+"
#define FILE_MODE_READ_APPEND_TRUNC     "a+"

#define HEADER_FILENAME_KEY        "filename=" 

enum input_method {
        INPUT_CLI,
        INPUT_FILE
};

enum download_mode {
        DOWNLOAD_ASYNC,
        DOWNLOAD_SYNC
};

enum dm_status {
        DM_OK,
        DM_FAIL
};

enum bytes_unit_idx {
        GB_UNIT_IDX,
        MB_UNIT_IDX,
        KB_UNIT_IDX,
        BYTES_UNIT_IDX
};

enum bits_unit_idx {
        GBPS_UNIT_IDX,
        MBPS_UNIT_IDX,
        KBPS_UNIT_IDX,
        BITS_UNIT_IDX 
};

struct dm_context  {
        int input_method;
        char input_buf[INPUT_BUF_SIZE];
        char *input_filepath, *dirpath;
};

struct dm_download {
        int mode;

        CURL *curl;
        FILE *file;

        bool has_filename;

        size_t count, idx;
        char **url, **filename, **filepath;

        const char *current_filename;
};

static const char *const size_unit[] = {
        "GiB",
        "MiB",
        "KiB",
        "bytes"
};

static const char *const speed_unit[] = {
        "GiB/s",
        "MiB/s",
        "KiB/s",
        "bytes/s"
};

static const char *const speed_unit_bits[] = {
        "Gbps",
        "Mbps",
        "Kbps",
        "bits/s"
};

static const char dlstr[] = "#########################";
static size_t dlstr_len = ARR_LEN(dlstr) - 1;

size_t dm_header_cb(char *buffer,
                size_t size,
                size_t nitems,
                void *userdata);
size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp);
int dm_progress_cb(void *clientp,
                curl_off_t dltotal,
                curl_off_t dlnow,
                curl_off_t ultotal,
                curl_off_t ulnow);
int dm_context_init(struct dm_context *dm_ctx, int input_method);
void dm_context_cleanup(struct dm_context *dm_ctx);
int dm_download_init(struct dm_download *dm_dl, CURL *curl, int mode);
void dm_download_cleanup(struct dm_download *dl);
int dm_download_perform(struct dm_context *dm_ctx, struct dm_download *dm_dl);
int dm_download_sync(struct dm_download *dm_dl,
                const char *path,
                const char *url);
int dm_parse_args(struct dm_context *dm_ctx,
                struct parg_state *ps,
                int argc,
                char *argv[],
                const char *arg_fmt);
int dm_validate_argc(int argc);
int dm_validate_args(struct dm_context *dm_ctx, size_t arg_start_idx, int argc);
int dm_input_cli(struct dm_download *dm_dl,
                size_t arg_start_idx,
                int argc,
                char *argv[]);
int dm_input_file(struct dm_context *dm_ctx,
                struct dm_download *dm_dl,
                FILE *input_file);
int dm_conv_size(double *size, const char **unit);
int dm_conv_speed(double *speed, const char **unit);
void dm_format_time(unsigned long seconds, char *buf, size_t buf_size);
void dm_log_version(void);
void dm_log_usage(void);
void dm_log_help(void);
void dm_log_download_progress(struct dm_download *dm_dl,
                curl_off_t dltotal,
                curl_off_t dlnow);
#endif
