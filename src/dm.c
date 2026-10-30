#include "dm.h"
#include "log.h"
#include "ascii.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <math.h>
#ifdef _WIN32 
#else
#include <unistd.h>
#endif

char err_buf[ERR_BUF_SIZE];
const char *exec_name;

int main(int argc, char *argv[])
{
        int retval = EXIT_FAILURE;

        FILE *input_file = NULL;

        CURL *curl;
        CURLcode res_init, res_setopt;

        struct dm_context dm_ctx;
        struct dm_download dm_dl;

        exec_name = base_path(argv[0]);

        if (dm_context_init(&dm_ctx, INPUT_CLI) < 0)
                goto out;

        dm_parse_args(&dm_ctx, argc, argv, ARG_FMT);
        if (dm_validate_args(&dm_ctx, optind, argc) < 0)
                goto out_dm_cleanup;

        res_init = curl_global_init(CURL_GLOBAL_ALL);
        if (res_init != CURLE_OK)
                goto out_dm_cleanup;

        curl = curl_easy_init();
        if (!curl)
                goto out_curl_cleanup;

        dm_download_init(&dm_dl, curl);

        res_setopt = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buf);
        if (res_setopt != CURLE_OK)
                goto out_dl_cleanup;

        res_setopt = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOWLOCATION_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dm_data_cb);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dm_dl);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        } 

        if (dm_ctx.input_method == INPUT_CLI) {
                if (dm_input_cli(&dm_dl, optind, argc, argv) < 0)
                        goto out_dl_cleanup;
        } else {
                input_file = fopen(dm_ctx.input_filepath, "rb");
                if (!input_file) {
                        LOG_FATAL(strerror(errno));
                        goto out_dl_cleanup;
                }

                if (dm_input_file(&dm_ctx, &dm_dl, input_file) < 0)
                        goto out_dl_cleanup;

                if (input_file)
                        fclose(input_file);
        }

        if (dm_download(&dm_ctx, &dm_dl, DOWNLOAD_SYNC) < 0)
                goto out_dl_cleanup;


        /*
         * Appending a '\n' character in stdout
         * to ensure the following shell prompt
         * gets drawn on a new line. 
         */
        putc('\n', stdout);

        retval = EXIT_SUCCESS;


out_dl_cleanup:
        dm_download_cleanup(&dm_ctx, &dm_dl);
out_curl_cleanup:
        curl_global_cleanup();
out_dm_cleanup:
        dm_context_cleanup(&dm_ctx);
out:
        return retval;
}

size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp)
{
        struct dm_download *dl = userp;

        dm_log_download_progress(dl);

        /*
         * Pass 1 as the element size so fwrite's return value
         * aligns with libcurl's expectations of bytes written.
         */
        return fwrite(buffer, 1, size * nmemb, dl->file);
}

int dm_context_init(struct dm_context *dm_ctx, int input_method)
{
        int retval = -1;

        if (!dm_ctx)
                goto out;

        dm_ctx->input_method = input_method;
        dm_ctx->input_filepath = NULL;
        dm_ctx->dirpath = NULL;

        retval = 0;

out:
        return retval;
}

void dm_context_cleanup(struct dm_context *dm_ctx)
{
        if (dm_ctx->input_filepath)
                free(dm_ctx->input_filepath);
        if (dm_ctx->dirpath)
                free(dm_ctx->dirpath);

}

int dm_download_init(struct dm_download *dl, CURL *curl)
{
        int retval = -1;

        if (!dl || !curl)
                goto out;

        dl->curl = curl;
        dl->file = NULL;
        dl->url_count = 0;
        dl->url = NULL;
        dl->filename = NULL;
        dl->filepath = NULL;

        retval = 0;

out:
        return retval;
}

void dm_download_cleanup(struct dm_context *dm_ctx, struct dm_download *dl)
{
        if (dl->filepath) {
                for (size_t i = 0; i < dl->url_count; i++)
                        free(dl->filepath[i]);
                free(dl->filepath);
        }

        if (dl->filename)
                free(dl->filename);

        if (dl->url) {
                if (dm_ctx->input_method == INPUT_FILE)
                        for (size_t i = 0; i < dl->url_count; i++)
                                free(dl->url[i]);
                free(dl->url);
        }

        if (dl->curl)
                curl_easy_cleanup(dl->curl);
}

int dm_download(struct dm_context *dm_ctx, struct dm_download *dl, int download_mode)
{
        int retval = -1;

        char *temp;

        if (download_mode != DOWNLOAD_SYNC)
                goto out;

        for (size_t i = 0; i < dl->url_count; i++) {
                if (dm_ctx->dirpath) {
                        size_t size = strlen(dm_ctx->dirpath) + strlen(dl->filename[i]) + 1;
                        temp = malloc(size);
                        if (!temp)
                                goto out;

                        strcpy(temp, dm_ctx->dirpath);
                        strcat(temp, dl->filename[i]);

                        dl->filepath[i] = temp;
                }


                if (dm_download_sync(dl, dl->filepath[i], dl->url[i]) < 0) {
                        LOG_FATAL_VA("download failed (%s): %s", dl->filename[i], err_buf);
                        goto out;
                }
        }

        retval = 0;

out:
        return retval;
}

int dm_download_sync(struct dm_download *dl, const char *path, const char *url)
{
        int retval = -1;

        CURLcode res_setopt, res_perform;

        FILE *file = fopen(path, "wb");
        if (!file) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dl->file = file;
        dl->current_filename = base_path(path);

        res_setopt = curl_easy_setopt(dl->curl, CURLOPT_URL, url);
        if (res_setopt != CURLE_OK)
                goto out;

        res_perform = curl_easy_perform(dl->curl);
        if (res_perform != CURLE_OK)
                goto out;

        retval = 0;

out:
        return retval;
}

void dm_parse_args(struct dm_context *dm_ctx, int argc, char *argv[], const char *arg_fmt)
{
        int opt;
        size_t optarg_len;
        bool slash = false;

        while ((opt = getopt(argc, argv, arg_fmt)) != -1) {
                switch (opt) {
                        case 'v':
                                dm_log_version();
                                exit(EXIT_SUCCESS);
                        case 'h':
                                dm_log_helpstr();
                                exit(EXIT_SUCCESS);
                        case 'd':
                                optarg_len = strlen(optarg);
                                if (optarg_len == 0) {
                                        LOG_ERROR("path argument cannot be empty");
                                        exit(EXIT_FAILURE);
                                }

                                slash = false;
                                if (optarg[optarg_len - 1] == '/')
                                        slash = true;

                                if (slash) {
                                        dm_ctx->dirpath = strdup(optarg);
                                        if (!dm_ctx->dirpath)
                                                exit(EXIT_FAILURE);
                                        break;
                                }

                                size_t size = optarg_len + 2;
                                dm_ctx->dirpath = malloc(size);
                                if (!dm_ctx->dirpath)
                                        exit(EXIT_FAILURE);

                                memcpy(dm_ctx->dirpath, optarg, size - 2);
                                dm_ctx->dirpath[size - 2] = '/';
                                dm_ctx->dirpath[size - 1] = '\0';

                                break;
                        case 'f':
                                dm_ctx->input_method = INPUT_FILE;
                                dm_ctx->input_filepath = strdup(optarg);
                                if (!dm_ctx->input_filepath)
                                        exit(EXIT_FAILURE);

                                break;
                        case ':': 
                                LOG_ERROR_VA("'%c' option requires a value", optopt); 
                                exit(EXIT_FAILURE);
                        case '?': 
                                LOG_ERROR_VA("unknown option: '%c'", optopt);
                                exit(EXIT_FAILURE);
                        default:
                                LOG_ERROR("unknown parsing error");
                                exit(EXIT_FAILURE);
                }
        }
}

int dm_validate_args(struct dm_context *dm_ctx, int url_start_idx, int argc)
{
        int retval = -1;

        bool margc = argc >= MIN_ARGC;
        if (!margc || (url_start_idx >= argc &&
                        dm_ctx->input_method != INPUT_FILE)) {
                dm_log_helpstr();
                goto out;
        }

        retval = 0;

out:
        return retval;
}

int dm_input_cli(struct dm_download *dl, int url_start_idx, int argc, char *argv[])
{
        int retval = -1;

        dl->url_count = (size_t)(argc - url_start_idx);

        dl->url = malloc(dl->url_count * sizeof *dl->url);
        if (!dl->url) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dl->filename = malloc(dl->url_count * sizeof *dl->filename);
        if (!dl->filename) {
                retval = -2;
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dl->filepath = malloc(dl->url_count * sizeof *dl->filepath);
        if (!dl->filepath) {
                retval = -3;
                LOG_FATAL(strerror(errno));
                goto out;
        }

        for (size_t i = 0; i < dl->url_count; i++) {
                dl->url[i] = argv[url_start_idx + i];
                dl->filename[i] = (char*)base_path(dl->url[i]);
                dl->filepath[i] = dl->filename[i];
        }

        retval = 0;

out:
        return retval;
}

int dm_input_file(struct dm_context *dm_ctx, struct dm_download *dl, FILE *input_file)
{
        int retval = -1;

        char *temp;
        size_t url_cap = URL_BASE_CAP;

        dl->url = malloc(url_cap * sizeof *dl->url);
        dl->filename = malloc(url_cap * sizeof *dl->filename);
        dl->filepath = malloc(url_cap * sizeof *dl->filepath);

        while (fgets(dm_ctx->input_buf, INPUT_BUF_SIZE, input_file)) {
                temp = strrchr(dm_ctx->input_buf, '\n');
                *temp = '\0';

                if (dl->url_count >= url_cap) {
                        url_cap *= URL_GROWTH_FACT;

                        temp = realloc(dl->url, url_cap);
                        if (!temp)
                                goto out;

                        dl->url = (char**)temp;

                        temp = realloc(dl->filename, url_cap);
                        if (!temp)
                                goto out;

                        dl->filename = (char**)temp;

                        temp = realloc(dl->filepath, url_cap);
                        if (!temp)
                                goto out;

                        dl->filepath = (char**)temp;
                }

                dl->url[dl->url_count] = strdup(dm_ctx->input_buf);
                if (!dl->url[dl->url_count])
                        goto out;

                dl->filename[dl->url_count] = (char*)base_path(dl->url[dl->url_count]);
                dl->filepath[dl->url_count] = dl->filename[dl->url_count];

                dl->url_count++;
        }

        retval = 0;
out:
        return retval;
}

int dm_conv_size(double *size, const char **suffix)
{
        int retval = 0;

        if (!size || !suffix) {
                retval = -1;
                goto out;
        }

        /* 
         * Do not change the order of these
         * if statements as the conversion logic
         * has been encoded within the order 
         * of these statements.
         */
        if (*size >= BYTES_PER_GIB) {
                *size /= BYTES_PER_GIB;
                *suffix = size_suffix[GB_SUFFIX_IDX];
                goto out;
        }

        if (*size >= BYTES_PER_MIB) {
                *size /= BYTES_PER_MIB;
                *suffix = size_suffix[MB_SUFFIX_IDX];
                goto out;
        }

        if (*size >= BYTES_PER_KIB) {
                *size /= BYTES_PER_KIB;
                *suffix = size_suffix[KB_SUFFIX_IDX];
                goto out;
        }

        /* base case */
        *suffix = size_suffix[BYTES_SUFFIX_IDX];

out:
        return retval;
}

int dm_conv_speed(double *speed, const char **suffix)
{
        int retval = 0;

        if (!speed || !suffix) {
                retval = -1;
                goto out;
        }

        /* 
         * Do not change the order of these
         * if statements as the conversion logic
         * has been encoded within the order 
         * of these statements.
         */
        if (*speed >= BYTES_PER_GIB) {
                *speed /= BYTES_PER_GIB;
                *suffix = speed_suffix[GB_SUFFIX_IDX];
                goto out;
        }

        if (*speed >= BYTES_PER_MIB) {
                *speed /= BYTES_PER_MIB;
                *suffix = speed_suffix[MB_SUFFIX_IDX];
                goto out;
        }

        if (*speed >= BYTES_PER_KIB) {
                *speed /= BYTES_PER_KIB;
                *suffix = speed_suffix[KB_SUFFIX_IDX];
                goto out;
        }

        /* base case */
        *suffix = speed_suffix[BYTES_SUFFIX_IDX];

out:
        return retval;
}

void dm_log_version()
{
        LOG_VA("version %d.%d", VERSION_MAJOR, VERSION_MINOR);
}

void dm_log_helpstr()
{
        LOG_VA("usage: %s [options] [URI]...", exec_name);
        LOG("options:");
        LOG("\t-v: display version number.");
        LOG("\t-h: display this help message.");
        LOG("\t-d <path>: specify a directory (does not create the directory!).");
        LOG("\t-f <file>: specify a file containing URI.");
}

void dm_log_download_progress(struct dm_download *dl)
{
        curl_off_t speed, filesize;
        double cspeed, csize, cfilesize;
        const char *speed_suffix, *size_suffix, *filesize_suffix;

        double progress;
        int percent;
        int progress_len;

        CURLcode res_speed, res_filesize; 

        if (!dl)
                goto out;

        res_speed = curl_easy_getinfo(dl->curl, CURLINFO_SPEED_DOWNLOAD_T, &speed);
        res_filesize = curl_easy_getinfo(dl->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &filesize);

        if (res_speed != CURLE_OK)
                goto out;

        long pos = ftell(dl->file);
        if (pos <= 0)
                goto out;

        cspeed = (double)speed;
        csize = (double)pos;

        if (dm_conv_speed(&cspeed, &speed_suffix) < 0)
                goto out;
        if (dm_conv_size(&csize, &size_suffix) < 0)
                goto out;

        if (res_filesize != CURLE_OK || filesize <= 0) {
                printf(CLEAR_LINE "%s (%.2f %s / %s) %.2f %s",
                                dl->current_filename,
                                csize, size_suffix,
                                "unknown",
                                cspeed, speed_suffix);
                fflush(stdout);
                goto out;
        }

        cfilesize = filesize;

        dm_conv_size(&cfilesize, &filesize_suffix);

        progress = (double)pos / filesize;
        percent = (int)roundf(progress * 100.0);
        progress_len = (int)roundf(progress * dlstr_len);

        printf(CLEAR_LINE "%s (%.2f %s / %.2f %s) %.2f %s %d%% [%-*.*s]",
                        dl->current_filename,
                        csize, size_suffix,
                        cfilesize, filesize_suffix,
                        cspeed, speed_suffix,
                        percent,
                        (int)dlstr_len, progress_len, dlstr);
        fflush(stdout);

out:
        return;
}
