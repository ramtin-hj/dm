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
        exec_name = base_path(argv[0]);

        struct download dl;

        char *temp;

        int input_method = INPUT_METHOD_CLI;
        char input_buf[INPUT_BUF_SIZE];
        char *input_filepath = NULL;
        char *dirpath = NULL;
        FILE *input_file = NULL;

        size_t url_count = 0;
        char **url = NULL;
        char **filename = NULL;
        char **filepath = NULL;

        CURL *curl;
        CURLcode res_init, res_setopt;
        bool slash = false;
        bool margc = argc >= MIN_ARGC;

        int opt;
        size_t optarg_len;

        int retval = EXIT_FAILURE;

        if (!margc) {
                dm_log_helpstr();
                goto out;
        }

        /* TODO: Implement a Windows alternative to getopt to facilitate cross-compatability. */
        while ((opt = getopt(argc, argv, ":vhd:f:")) != -1) {
                switch (opt) {
                        case 'v':
                                dm_log_version();
                                goto out;
                        case 'h':
                                dm_log_helpstr();
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
                        case 'f':
                                input_method = INPUT_METHOD_FILE;
                                input_filepath = strdup(optarg);
                                if (!input_filepath)
                                        goto out_free_dirpath;

                                break;
                        case ':': 
                                LOG_ERROR_VA("'%c' option requires a value", optopt); 
                                goto out;
                        case '?': 
                                LOG_ERROR_VA("unknown option: '%c'", optopt);
                                goto out;
                }
        }

        if (optind >= argc && input_method != INPUT_METHOD_FILE) {
                dm_log_helpstr();
                goto out_free_input_filepath;
        }

        res_init = curl_global_init(CURL_GLOBAL_ALL);
        if (res_init != CURLE_OK)
                goto out_free_input_filepath;

        curl = curl_easy_init();
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

        if (input_method == INPUT_METHOD_FILE) {
                input_file = fopen(input_filepath, "rb");
                if (!input_file) {
                        LOG_FATAL(strerror(errno));
                        goto out_curl;
                }

                size_t url_cap = URL_BASE_CAP;

                url = malloc(url_cap * sizeof *url);
                filename = malloc(url_cap * sizeof *filename);
                filepath = malloc(url_cap * sizeof *filepath);

                while (fgets(input_buf, INPUT_BUF_SIZE, input_file)) {
                        temp = strrchr(input_buf, '\n');
                        *temp = '\0';

                        if (url_count >= url_cap) {
                                url_cap *= URL_GROWTH_FACT;

                                temp = realloc(url, url_cap);
                                if (!temp)
                                        goto out_free_filepath;

                                url = (char**)temp;

                                temp = realloc(filename, url_cap);
                                if (!temp)
                                        goto out_free_filepath;

                                filename = (char**)temp;

                                temp = realloc(filepath, url_cap);
                                if (!temp)
                                        goto out_free_filepath;

                                filepath = (char**)temp;
                        }

                        url[url_count] = strdup(input_buf);
                        if (!url[url_count])
                                goto out_free_filepath;

                        filename[url_count] = (char*)base_path(url[url_count]);
                        filepath[url_count] = filename[url_count];
                        
                        url_count++;
                }

                for (size_t i = 0; i < url_count; i++) {
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

                fclose(input_file);
        } else {
                url_count = (size_t)(argc - optind);

                url = malloc(url_count * sizeof *url);
                if (!url) {
                        LOG_FATAL(strerror(errno));
                        goto out_curl;
                }

                filename = malloc(url_count * sizeof *filename);
                if (!filename) {
                        LOG_FATAL(strerror(errno));
                        goto out_free_url;
                }

                filepath = malloc(url_count * sizeof *filepath);
                if (!filepath) {
                        LOG_FATAL(strerror(errno));
                        goto out_free_filename;
                }

                for (size_t i = 0; i < url_count; i++) {
                        url[i] = argv[optind + i];
                        filename[i] = (char*)base_path(url[i]);
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
        }

        /*
         * Appending a '\n' character in stdout
         * to ensure the following shell prompt
         * gets drawn on a new line. 
         */
        putc('\n', stdout);

        retval = EXIT_SUCCESS;

out_free_filepath:
        if (filepath)
                free(filepath);
out_free_filename:
        if (filename)
                free(filename);
out_free_url:
        if (url) {
                if (input_method == INPUT_METHOD_FILE)
                        for (size_t i = 0; i < url_count; i++)
                                free(url[i]);
                free(url);
        }
out_curl:
        curl_easy_cleanup(curl);
out_curl_global:
        curl_global_cleanup();
out_free_input_filepath:
        if (input_filepath)
                free(input_filepath);
out_free_dirpath:
        if (dirpath)
                free(dirpath);
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
        LOG_VA("usage: %s [options] <URL>...", exec_name);
        LOG("options:");
        LOG("\t-v: display version number.");
        LOG("\t-h: display this help message.");
        LOG("\t-d <path>: specify a directory (does not create the directory!).");
}

void dm_log_download_progress(struct download *dl)
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
                                dl->filename,
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

        dm_log_download_progress(dl);

        /*
         * Pass 1 as the element size so fwrite's return value
         * aligns with libcurl's expectations of bytes written.
         */
        return fwrite(buffer, 1, size * nmemb, dl->file);
}

int dm_download_sync(struct download *dl, const char *path, const char *url)
{
        int retval = -1;

        CURLcode res_setopt, res_perform;

        FILE *file = fopen(path, "wb");
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
