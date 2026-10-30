#include "dm.h"
#include "log.h"
#include "ascii.h"
#include "util.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <curl/curl.h>
#ifdef _WIN32 
/* TODO: Implement a Windows approach to argument parsing. */
#else
#include <unistd.h>
#endif

char err_buf[ERR_BUF_SIZE];
const char *exec_name;

int main(int argc, char *argv[])
{
        int retval = EXIT_FAILURE;

        int opt;
        size_t optarg_len;

        /* Path must be initialized to null. */
        char *temp;
        char *dirpath = NULL;
        exec_name = base_path(argv[0]);

        struct download dl;

        CURLcode res_init, res_setopt;
        bool slash = false;
        bool margc = argc >= MIN_ARGC;

        if (!margc) {
                dm_print_helpstr();
                goto out;
        }

        while ((opt = getopt(argc, argv, ":vhd:")) != -1) {
                switch (opt) {
                        case 'v':
                                dm_print_version();
                                goto out;
                        case 'h':
                                dm_print_helpstr();
                                goto out;
                        case 'd':
                                optarg_len = strlen(optarg);
                                if (optarg_len == 0) {
                                        LOG_ERROR("path argument cannot be empty");
                                        goto out;
                                }

                                slash = false;
                                if (optarg[optarg_len - 1] == '/')
                                        slash = true;

                                if (slash) {
                                        dirpath = strdup(optarg);
                                        if (!dirpath)
                                                goto out;
                                        break;
                                }

                                size_t size = optarg_len + 2;
                                dirpath = malloc(size);
                                if (!dirpath)
                                        goto out;

                                memcpy(dirpath, optarg, size - 2);
                                dirpath[size - 2] = '/';
                                dirpath[size - 1] = '\0';

                                break;
                        case ':': 
                                LOG_ERROR("option requires a value"); 
                                goto out;
                        case '?': 
                                LOG_ERROR_VA("unknown option: '%c'", optopt);
                                goto out;
                }
        }

        if (optind >= argc) {
                dm_print_helpstr();
                goto out_free_dirpath;
        }

        res_init = curl_global_init(CURL_GLOBAL_ALL);
        if (res_init != CURLE_OK)
                goto out_free_dirpath;

        CURL *curl = curl_easy_init();
        if (!curl)
                goto out_curl_global;

        dl.curl = curl;

        res_setopt = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buf);
        if (res_setopt != CURLE_OK)
                goto out_curl;

        res_setopt = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOWLOCATION_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_curl;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dm_data_cb);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_curl;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dl);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_curl;
        } 

        size_t nurl = (size_t)(argc - optind);

        const char **url = malloc(nurl * sizeof *url);
        if (!url) {
                LOG_FATAL(strerror(errno));
                goto out_curl;
        }

        const char **filename = malloc(nurl * sizeof *filename);
        if (!filename) {
                LOG_FATAL(strerror(errno));
                goto out_free_urlstr;
        }

        const char **filepath = malloc(nurl * sizeof *filepath);
        if (!filepath) {
                LOG_FATAL(strerror(errno));
                goto out_free_filename;
        }

        for (size_t i = 0; i < nurl; i++) {
                url[i] = argv[optind + i];
                filename[i] = base_path(url[i]);
                filepath[i] = filename[i];

                if (dirpath) {
                        size_t size = strlen(dirpath) + strlen(filename[i]) + 1;
                        temp = realloc(dirpath, size);
                        if (!temp)
                                goto out_free_filepath;

                        dirpath = temp;

                        strcat(dirpath, filename[i]);

                        filepath[i] = dirpath;
                }

                if (dm_download_sync(&dl, filepath[i], url[i]) < 0) {
                        LOG_FATAL_VA("download failed (%s): %s", filename[i], err_buf);
                        goto out_free_filepath;
                }
        }

        /*
         * Appending a '\n' character in stdout
         * to ensure the following shell prompt
         * gets drawn on a new line. 
         */
        putc('\n', stdout);

        retval = EXIT_SUCCESS;

out_free_filepath:
        free(filepath);
out_free_filename:
        free(filename);
out_free_urlstr:
        free(url);
out_curl:
        curl_easy_cleanup(curl);
out_curl_global:
        curl_global_cleanup();
out_free_dirpath:
        if (dirpath) free(dirpath);
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

void dm_print_version()
{
        LOG_VA("version %d.%d", VERSION_MAJOR, VERSION_MINOR);
}

void dm_print_helpstr()
{
        LOG_VA("usage: %s [options] <URL>...", exec_name);
        LOG("options:");
        LOG("\t-v: display version number.");
        LOG("\t-h: display this help message.");
        LOG("\t-d <path>: specify a directory (does not create the directory!).");
}

void dm_print_progress(struct download *dl)
{
        if (!dl)
                goto out;

        curl_off_t speed, filesize;
        CURLcode res_speed, res_filesize; 

        res_speed = curl_easy_getinfo(dl->curl, CURLINFO_SPEED_DOWNLOAD_T, &speed);
        res_filesize = curl_easy_getinfo(dl->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &filesize);

        if (res_speed != CURLE_OK)
                goto out;

        long pos = ftell(dl->file);
        if (pos <= 0)
                goto out;

        double cspeed = (double)speed;
        double csize = (double)pos;
        const char *speed_suffix;
        const char *size_suffix;

        if (dm_conv_speed(&cspeed, &speed_suffix) < 0)
                goto out;
        if (dm_conv_size(&csize, &size_suffix) < 0)
                goto out;

        if (res_filesize != CURLE_OK || filesize <= 0) {
                printf(CLEAR_LINE "%s (%.2f %s / %s) %.2f %s",
                                dl->filename,
                                csize, size_suffix,
                                "unknown",
                                cspeed, speed_suffix);
                fflush(stdout);
                goto out;
        }

        double cfilesize = filesize;
        const char *filesize_suffix;

        dm_conv_size(&cfilesize, &filesize_suffix);

        double progress = (double)pos / filesize;
        int percent = (int)roundf(progress * 100.0);
        int progress_len = (int)roundf(progress * dlstr_len);

        printf(CLEAR_LINE "%s (%.2f %s / %.2f %s) %.2f %s %d%% [%-*.*s]",
                        dl->filename,
                        csize, size_suffix,
                        cfilesize, filesize_suffix,
                        cspeed, speed_suffix,
                        percent,
                        (int)dlstr_len, progress_len, dlstr);
        fflush(stdout);

out:
        return;
}

size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp)
{
        struct download *dl = userp;

        dm_print_progress(dl);

        /*
         * Pass 1 as the element size so fwrite's return value
         * aligns with libcurl's expectations of bytes written.
         */
        return fwrite(buffer, 1, size * nmemb, dl->file);
}

int dm_download_sync(struct download *dl, const char *path, const char *url)
{
        int retval = -1;

        FILE *file = fopen(path, "wb");
        CURLcode res_setopt, res_perform;

        if (!file) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dl->file = file;
        dl->filename = base_path(path);

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
