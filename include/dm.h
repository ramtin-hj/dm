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

#define BYTES_PER_GIB      1073741824ULL
#define BYTES_PER_MIB      1048576ULL
#define BYTES_PER_KIB      1024ULL

#define FOLLOWLOCATION_FLAG   1L

/* TODO: Implement file-reading functionality. */
enum input_method {
        INPUT_METHOD_CLI,
        INPUT_METHOD_FILE
};

struct download {
        CURL *curl;
        FILE *file;
        const char *filename;
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

int dm_conv_size(double *size, const char **suffix);
int dm_conv_speed(double *speed, const char **suffix);
void dm_log_version();
void dm_log_helpstr();
void dm_log_download_progress(struct download *dl);
size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp);
int dm_download_sync(struct download *dl, const char *path, const char *url);

#endif
