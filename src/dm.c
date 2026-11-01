#include "dm.h"
#include "log.h"
#include "ascii.h"
#include "util.h"
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <parg/parg.h>

char time_buf[TIME_BUF_SIZE];
char err_buf[ERR_BUF_SIZE];

const char *exec_name;

int main(int argc, char *argv[])
{
        int retval = EXIT_FAILURE;
        int optend = parg_reorder(argc, argv, ARG_FMT, NULL);

        FILE *input_file = NULL;

        CURL *curl;
        CURLcode res_init, res_setopt;

        struct dm_context dm_ctx;
        struct dm_download dm_dl;
        struct parg_state ps;

        exec_name = base_path(argv[0]);

        if (dm_context_init(&dm_ctx, INPUT_CLI) != DM_OK)
                goto out;

        if (dm_validate_argc(argc) != DM_OK)
                goto out;

        parg_init(&ps);

        dm_parse_args(&dm_ctx, &ps, optend, argv, ARG_FMT);
        if (dm_validate_args(&dm_ctx, ps.optind, argc) != DM_OK)
                goto out_dm_cleanup;

        res_init = curl_global_init(CURL_GLOBAL_ALL);
        if (res_init != CURLE_OK)
                goto out_dm_cleanup;

        curl = curl_easy_init();
        if (!curl)
                goto out_curl_cleanup;

        dm_download_init(&dm_dl, curl, DOWNLOAD_SYNC);

        res_setopt = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_buf);
        if (res_setopt != CURLE_OK)
                goto out_dl_cleanup;

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

        res_setopt = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, dm_progress_cb);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &dm_dl);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        } 

        res_setopt = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, !PROGRESS_BAR_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, LOW_SPEED_LIMIT_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, LOW_SPEED_TIME_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        res_setopt = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, FOLLOWLOCATION_FLAG);
        if (res_setopt != CURLE_OK) {
                LOG_FATAL(err_buf);
                goto out_dl_cleanup;
        }

        if (dm_ctx.input_method == INPUT_CLI) {
                if (dm_input_cli(&dm_dl, ps.optind, argc, argv) != DM_OK)
                        goto out_dl_cleanup;
        } else {
                input_file = fopen(dm_ctx.input_filepath, FILE_MODE_READ_BINARY);
                if (!input_file) {
                        LOG_FATAL(strerror(errno));
                        goto out_dl_cleanup;
                }

                if (dm_input_file(&dm_ctx, &dm_dl, input_file) != DM_OK)
                        goto out_dl_cleanup;

                if (input_file)
                        fclose(input_file);
        }

        if (dm_download_perform(&dm_ctx, &dm_dl) != DM_OK)
                goto out_dl_cleanup;


        /*
         * Appending a new-line character in stdout
         * to ensure the following shell prompt gets
         * drawn on a new line. 
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

int dm_progress_cb(void *clientp,
                curl_off_t dltotal,
                curl_off_t dlnow,
                curl_off_t ultotal,
                curl_off_t ulnow)
{
        struct dm_download *dm_dl = clientp;

        (void)ultotal;
        (void)ulnow;

        dm_log_download_progress(dm_dl, dltotal, dlnow);

        return CURLE_OK;
}

size_t dm_data_cb(char *buffer, size_t size, size_t nmemb, void *userp)
{
        struct dm_download *dm_dl = userp;

        /*
         * Pass 1 as the element size so fwrite's return value
         * aligns with libcurl's expectations of bytes written.
         */
        return fwrite(buffer, 1, size * nmemb, dm_dl->file);
}

int dm_context_init(struct dm_context *dm_ctx, int input_method)
{
        int retval = -DM_FAIL;

        if (!dm_ctx)
                goto out;

        dm_ctx->input_method = input_method;
        dm_ctx->input_filepath = NULL;
        dm_ctx->dirpath = NULL;

        retval = DM_OK;

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

int dm_download_init(struct dm_download *dm_dl, CURL *curl, int mode)
{
        int retval = -DM_FAIL;

        if (!dm_dl || !curl)
                goto out;

        dm_dl->curl = curl;
        dm_dl->file = NULL;
        dm_dl->count = 0;
        dm_dl->url = NULL;
        dm_dl->filename = NULL;
        dm_dl->filepath = NULL;
        dm_dl->mode = mode;

        retval = DM_OK;

out:
        return retval;
}

void dm_download_cleanup(struct dm_context *dm_ctx, struct dm_download *dm_dl)
{
        if (dm_dl->filepath)
                free(dm_dl->filepath);

        if (dm_dl->filename)
                free(dm_dl->filename);

        if (dm_dl->url) {
                if (dm_ctx->input_method == INPUT_FILE)
                        for (size_t i = 0; i < dm_dl->count; i++)
                                free(dm_dl->url[i]);
                free(dm_dl->url);
        }

        if (dm_dl->curl)
                curl_easy_cleanup(dm_dl->curl);
}

int dm_download_perform(struct dm_context *dm_ctx, struct dm_download *dm_dl)
{
        int retval = -DM_FAIL;

        char *temp;

        /* 
         * TODO: Implement asynchronous downloads
         * using curl's multimode.
         */
        if (dm_dl->mode != DOWNLOAD_SYNC)
                goto out;

        for (size_t i = 0; i < dm_dl->count; i++) {
                dm_dl->idx = i;
                if (dm_ctx->dirpath) {
                        size_t size = strlen(dm_ctx->dirpath) + strlen(dm_dl->filename[i]) + 1;
                        temp = malloc(size);
                        if (!temp)
                                goto out;

                        strcpy(temp, dm_ctx->dirpath);
                        strcat(temp, dm_dl->filename[i]);

                        dm_dl->filepath[i] = temp;
                }


                if (dm_download_sync(dm_dl, dm_dl->filepath[i], dm_dl->url[i]) != DM_OK) {
                        LOG_FATAL_VA("download failed (%s): %s", dm_dl->filename[i], err_buf);
                        goto out;
                }

                if (dm_ctx->dirpath) {
                        free(dm_dl->filepath[i]);
                        dm_dl->filepath[i] = NULL;
                }
        }

        retval = DM_OK;

out:
        return retval;
}

int dm_download_sync(struct dm_download *dm_dl,
                const char *path,
                const char *url)
{
        int retval = -DM_FAIL;

        CURLcode res_setopt, res_perform;

        FILE *file = fopen(path, FILE_MODE_WRITE_BINARY);
        if (!file) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dm_dl->file = file;
        dm_dl->current_filename = base_path(path);

        res_setopt = curl_easy_setopt(dm_dl->curl, CURLOPT_URL, url);
        if (res_setopt != CURLE_OK)
                goto out;

        res_perform = curl_easy_perform(dm_dl->curl);
        if (res_perform != CURLE_OK)
                goto out;

        retval = DM_OK;

out:
        return retval;
}

int dm_parse_args(struct dm_context *dm_ctx,
                struct parg_state *ps,
                int argc,
                char *argv[],
                const char *arg_fmt)
{
        int retval = -DM_FAIL;

        if (!dm_ctx || !ps)
                goto out;

        int opt;
        size_t optarg_len, new_size;
        bool slash = false;

        while ((opt = parg_getopt(ps, argc, argv, arg_fmt)) != -1) {
                switch (opt) {
                        case 'v':
                                dm_log_version();
                                exit(EXIT_SUCCESS);
                        case 'h':
                                dm_log_help();
                                exit(EXIT_SUCCESS);
                        case 'd':
                                /* 
                                 * TODO: Since Windows supports both forward and backwards
                                 * slashes in it's paths, a more exhaustive approach must
                                 * be taken for appending trailing slashes.
                                 */
                                optarg_len = strlen(ps->optarg);
                                if (optarg_len == 0) {
                                        LOG_ERROR("path argument cannot be empty");
                                        exit(EXIT_FAILURE);
                                }

                                if (optarg_len >= 1 && ps->optarg[optarg_len - 1] == '/')
                                        slash = true;

                                if (slash) {
                                        dm_ctx->dirpath = strdup(ps->optarg);
                                        if (!dm_ctx->dirpath)
                                                exit(EXIT_FAILURE);
                                        break;
                                }

                                /* 
                                 * Allocating enough size for the forward/backwards slash
                                 * as well as the null-terminating character. 
                                 */
                                new_size = optarg_len + 2;

                                dm_ctx->dirpath = malloc(new_size);
                                if (!dm_ctx->dirpath)
                                        exit(EXIT_FAILURE);

                                memcpy(dm_ctx->dirpath, ps->optarg, optarg_len);

                                dm_ctx->dirpath[new_size - 2] = '/';
                                dm_ctx->dirpath[new_size - 1] = '\0';

                                break;
                        case 'f':
                                dm_ctx->input_method = INPUT_FILE;

                                dm_ctx->input_filepath = strdup(ps->optarg);
                                if (!dm_ctx->input_filepath)
                                        exit(EXIT_FAILURE);

                                break;
                        case ':': 
                                LOG_ERROR_VA("'%c' option requires a value", ps->optopt); 
                                exit(EXIT_FAILURE);
                        case '?': 
                                LOG_ERROR_VA("unknown option: '%c'", ps->optopt);
                                exit(EXIT_FAILURE);
                        default:
                                LOG_ERROR("unknown parsing error");
                                exit(EXIT_FAILURE);
                }
        }

        retval = DM_OK;
out:
        return retval;
}

int dm_validate_argc(int argc)
{
        int retval = -DM_FAIL;

        if (argc < MIN_ARGC) {
                dm_log_usage();
                goto out;
        }

        retval = DM_OK;

out:
        return retval;
}

int dm_validate_args(struct dm_context *dm_ctx, int arg_start_idx, int argc)
{
        int retval = -DM_FAIL;

        if (arg_start_idx >= argc && dm_ctx->input_method != INPUT_FILE) {
                dm_log_help();
                goto out;
        }

        retval = DM_OK;

out:
        return retval;
}

int dm_input_cli(struct dm_download *dm_dl,
                int arg_start_idx,
                int argc,
                char *argv[])
{
        int retval = -DM_FAIL;

        dm_dl->count = (size_t)(argc - arg_start_idx);

        dm_dl->url = malloc(dm_dl->count * sizeof *dm_dl->url);
        if (!dm_dl->url) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dm_dl->filename = malloc(dm_dl->count * sizeof *dm_dl->filename);
        if (!dm_dl->filename) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        dm_dl->filepath = malloc(dm_dl->count * sizeof *dm_dl->filepath);
        if (!dm_dl->filepath) {
                LOG_FATAL(strerror(errno));
                goto out;
        }

        for (size_t i = 0; i < dm_dl->count; i++) {
                dm_dl->url[i] = argv[arg_start_idx + i];
                dm_dl->filename[i] = (char*)base_path(dm_dl->url[i]);
                dm_dl->filepath[i] = dm_dl->filename[i];
        }

        retval = 0;

out:
        return retval;
}

int dm_input_file(struct dm_context *dm_ctx,
                struct dm_download *dm_dl,
                FILE *input_file)
{
        int retval = -DM_FAIL;

        char *temp;
        size_t url_cap = DYN_ARR_BASE_CAP;

        dm_dl->url = malloc(url_cap * sizeof *dm_dl->url);
        dm_dl->filename = malloc(url_cap * sizeof *dm_dl->filename);
        dm_dl->filepath = malloc(url_cap * sizeof *dm_dl->filepath);

        while (fgets(dm_ctx->input_buf, INPUT_BUF_SIZE, input_file)) {
                temp = strrchr(dm_ctx->input_buf, '\n');
                *temp = '\0';

                if (dm_dl->count >= url_cap) {
                        url_cap *= DYN_ARR_GROWTH_FACT;

                        temp = realloc(dm_dl->url, url_cap);
                        if (!temp)
                                goto out;

                        dm_dl->url = (char**)temp;

                        temp = realloc(dm_dl->filename, url_cap);
                        if (!temp)
                                goto out;

                        dm_dl->filename = (char**)temp;

                        temp = realloc(dm_dl->filepath, url_cap);
                        if (!temp)
                                goto out;

                        dm_dl->filepath = (char**)temp;
                }

                dm_dl->url[dm_dl->count] = strdup(dm_ctx->input_buf);
                if (!dm_dl->url[dm_dl->count])
                        goto out;

                dm_dl->filename[dm_dl->count] = (char*)base_path(dm_dl->url[dm_dl->count]);
                dm_dl->filepath[dm_dl->count] = dm_dl->filename[dm_dl->count];

                dm_dl->count++;
        }

        retval = DM_OK;
out:
        return retval;
}

int dm_size_unit(double *size, const char **unit)
{
        int retval = DM_OK;

        if (!size || !unit) {
                retval = -DM_FAIL;
                goto out;
        }

        /* 
         * Do not change the order of these
         * statements as the conversion logic
         * has been encoded within the order 
         * of these statements.
         */
        if (*size >= BYTES_PER_GIB) {
                *size /= BYTES_PER_GIB;
                *unit = size_unit[GB_UNIT_IDX];
                goto out;
        }

        if (*size >= BYTES_PER_MIB) {
                *size /= BYTES_PER_MIB;
                *unit = size_unit[MB_UNIT_IDX];
                goto out;
        }

        if (*size >= BYTES_PER_KIB) {
                *size /= BYTES_PER_KIB;
                *unit = size_unit[KB_UNIT_IDX];
                goto out;
        }

        /* base case */
        *unit = size_unit[BYTES_UNIT_IDX];

out:
        return retval;
}

int dm_speed_unit(double *speed, const char **unit)
{
        int retval = DM_OK;

        if (!speed || !unit) {
                retval = -DM_FAIL;
                goto out;
        }

        /* 
         * Do not change the order of these
         * statements as the conversion logic
         * has been encoded within the order 
         * of these statements.
         */

        if (*speed >= BYTES_PER_GIB) {
                *speed /= BYTES_PER_GIB;
                *unit = speed_unit[GB_UNIT_IDX];
                goto out;
        }

        if (*speed >= BYTES_PER_MIB) {
                *speed /= BYTES_PER_MIB;
                *unit = speed_unit[MB_UNIT_IDX];
                goto out;
        }

        if (*speed >= BYTES_PER_KIB) {
                *speed /= BYTES_PER_KIB;
                *unit = speed_unit[KB_UNIT_IDX];
                goto out;
        }

        /* base case */
        *unit = speed_unit[BYTES_UNIT_IDX];

out:
        return retval;
}

int dm_speed_unit_bits(double *speed_bits, const char **unit)
{
        int retval = DM_OK;

        if (!speed_bits || !unit) {
                retval = -DM_FAIL;
                goto out;
        }

        /* 
         * Do not change the order of these
         * statements as the conversion logic
         * has been encoded within the order 
         * of these statements.
         */

        if (*speed_bits >= BITS_PER_GIGABIT) {
                *speed_bits /= BITS_PER_GIGABIT;
                *unit = speed_unit_bits[GBPS_UNIT_IDX];
                goto out;
        }

        if (*speed_bits >= BITS_PER_MEGABIT) {
                *speed_bits /= BITS_PER_MEGABIT;
                *unit = speed_unit_bits[MBPS_UNIT_IDX];
                goto out;
        }

        if (*speed_bits >= BITS_PER_KILOBIT) {
                *speed_bits /= BITS_PER_KILOBIT;
                *unit = speed_unit_bits[KBPS_UNIT_IDX];
                goto out;
        }

        /* base case */
        *unit = speed_unit_bits[BITS_UNIT_IDX];

out:
        return retval;
}

void dm_format_time(unsigned long seconds, char *buf, size_t buf_size) {
        if (seconds == 0 || seconds > SECONDS_PER_DAY) {
                /* Fallback for unknown, infinite, or excessively long ETAs (> 24 hours) */
                snprintf(buf, buf_size, "--:--");
                return;
        }

        unsigned long hours = seconds / SECONDS_PER_HOUR;
        unsigned long minutes = (seconds % SECONDS_PER_HOUR) / MINUTES_PER_HOUR;
        seconds = seconds % SECONDS_PER_MINUTE;

        if (hours > 0) {
                snprintf(buf, buf_size, "%02d:%02d:%02d",
                                (int)hours,
                                (int)minutes,
                                (int)seconds);
        } else
                snprintf(buf, buf_size, "%02d:%02d", (int)minutes, (int)seconds);
}

void dm_log_version(void)
{
        LOG_VA("version %d.%d", VERSION_MAJOR, VERSION_MINOR);
}

void dm_log_usage(void)
{
        LOG_VA("usage: %s [options] [URI]...", exec_name);
        LOG_VA("for more info use '%s -h'", exec_name);
}

void dm_log_help(void)
{
        LOG_VA("usage: %s [options] [URI]...", exec_name);
        LOG("options:");
        LOG("\t-v: display version number.");
        LOG("\t-h: display this help message.");
        LOG("\t-d <path>: specify a directory (does not create the directory!).");
        LOG("\t-f <file>: specify a file containing URI.");
}

void dm_log_download_progress(struct dm_download *dm_dl,
                curl_off_t dltotal,
                curl_off_t dlnow)
{
        curl_off_t dlspeed;

        double dlnow_conv, dltotal_conv, dlspeed_conv, dlspeed_bits_conv;
        const char *dlnow_unit, *dlspeed_unit, *dlspeed_bits_unit, *dltotal_unit;

        unsigned long dlremaining = 0;
        unsigned long eta_seconds = 0;

        double progress = 0;
        int percent = 0;
        int progress_len = 0;

        CURLcode res_speed; 

        if (!dm_dl)
                goto out;

        res_speed = curl_easy_getinfo(dm_dl->curl,
                        CURLINFO_SPEED_DOWNLOAD_T,
                        &dlspeed);
        if (res_speed != CURLE_OK)
                goto out;

        dlnow_conv = dlnow;
        dlspeed_conv = dlspeed;
        dlspeed_bits_conv = (double)dlspeed * BITS_PER_BYTE; 

        if (dm_size_unit(&dlnow_conv, &dlnow_unit) != DM_OK)
                goto out;
        if (dm_speed_unit(&dlspeed_conv, &dlspeed_unit) != DM_OK)
                goto out;
        if (dm_speed_unit_bits(&dlspeed_bits_conv, &dlspeed_bits_unit) != DM_OK)
                goto out;

        if (dltotal <= 0) {
                printf(ASCII_CLEAR_LINE
                                "[%zu/%zu] %s [%.2f %s / %s] %.2f %s (%.2f %s)",
                                dm_dl->idx + 1, dm_dl->count,
                                dm_dl->current_filename,
                                dlnow_conv, dlnow_unit,
                                "unknown",
                                dlspeed_conv, dlspeed_unit,
                                dlspeed_bits_conv, dlspeed_bits_unit);
                fflush(stdout);
                goto out;
        }

        dltotal_conv = dltotal;

        if (dm_size_unit(&dltotal_conv, &dltotal_unit) != DM_OK)
                goto out; 


        if (dltotal != 0L) {
                progress = (double)dlnow / (double)dltotal;
                percent = round(progress * 100.0);
                progress_len = round(progress * dlstr_len);
        }

        if (dlnow <= dltotal) {
                dlremaining = dltotal - dlnow;
                if (dlspeed != 0L)
                        eta_seconds = (double)dlremaining / (double)dlspeed;
        }

        dm_format_time(eta_seconds, time_buf, TIME_BUF_SIZE);

        printf(ASCII_CLEAR_LINE 
                        "[%zu/%zu] %s [%.2f %s / %.2f %s] %d%% [%-*.*s] %.2f %s (%.2f %s) - %s ETA",
                        dm_dl->idx + 1, dm_dl->count,
                        dm_dl->current_filename,
                        dlnow_conv, dlnow_unit,
                        dltotal_conv, dltotal_unit,
                        percent,
                        (int)dlstr_len, progress_len, dlstr,
                        dlspeed_conv, dlspeed_unit,
                        dlspeed_bits_conv, dlspeed_bits_unit,
                        time_buf);
        fflush(stdout);

out:
        return;
}
