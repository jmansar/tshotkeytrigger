#include "datafile.h"
#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static error_t load_api_key_file_path(char *result, size_t result_buf_len) {
  char *home_path = getenv("HOME");
  char *base_path = getenv("XDG_DATA_HOME");
  size_t buffer_used_len = 0;

  if (base_path == NULL || strlen(base_path) == 0) {
    buffer_used_len = lws_snprintf(result, result_buf_len, "%s/%s", home_path,
                                   ".local/share");
    if (buffer_used_len >= result_buf_len - 1) {
      lwsl_err("data file path exceeds max length");
      return ERR_DATA_PATH_EXCEED_MAX_PATH;
    }
  } else {
    buffer_used_len = lws_snprintf(result, result_buf_len, "%s", base_path);
  }

  buffer_used_len =
      lws_snprintf(result + buffer_used_len, result_buf_len - buffer_used_len,
                   "%s", "/tshotkeysctl/api_key");

  if (buffer_used_len >= result_buf_len - 1) {
    lwsl_err("api key data file path exceeds max length");
    return ERR_DATA_FILE_EXCEED_MAX_PATH;
  }

  lwsl_notice("api key file path: %s", result);

  return NO_ERROR;
}

static void ensure_data_dir_present(const char *data_path) {
  const char separator = '/';

  const char *ret = data_path;
  int index = 0;
  struct stat stat_struct = {0};

  while (ret != NULL && ret[0] != '\0') {
    ret = strchr(&data_path[ret - data_path], separator);

    if (ret == NULL) {
      break;
    }

    char *dir = strdup(data_path);
    index = ret - data_path;

    dir[index + 1] = 0;

    if (stat(dir, &stat_struct)) {
      lwsl_notice("creating path: %s", dir);

      mkdir(dir, 0700);
    }

    free(dir);

    ret++;
  }
}

static void load_api_key(const char *api_key_file_path, char *result,
                         size_t result_buf_len) {
  FILE *file;

  lwsl_notice("Loading API key");

  file = fopen(api_key_file_path, "r");

  if (file) {
    fgets(result, result_buf_len, file);
    fclose(file);
    lwsl_notice("API key loaded");
  } else {
    lwsl_notice("API key file not found");
  }
}

const char *df_get_api_key(df_data_t *data) { return data->api_key; }

error_t df_init(df_data_t *data) {
  error_t result_code =
      load_api_key_file_path(data->api_key_file_path, PATH_BUF_SIZE);
  if (result_code != 0) {
    return result_code;
  }

  load_api_key(data->api_key_file_path, data->api_key, API_KEY_BUF_SIZE);
  return NO_ERROR;
}

error_t df_save_api_key(df_data_t *data, const char *new_api_key) {
  error_t result_code = NO_ERROR;
  FILE *file;
  size_t api_key_len;

  if (strcmp(data->api_key, new_api_key) == 0) {
    lwsl_notice("API key is not changed. Continue...");
    return result_code;
  }

  api_key_len = strlen(new_api_key);
  if (api_key_len >= API_KEY_BUF_SIZE) {

    lwsl_warn("API key exceeds the maximum key size %d. Not saving to the file",
              API_KEY_BUF_SIZE - 1);
    return ERR_API_KEY_EXCEED_MAX_LEN;
  }

  lwsl_notice("New API key - save to: %s", data->api_key_file_path);

  ensure_data_dir_present(data->api_key_file_path);

  file = fopen(data->api_key_file_path, "w");
  if (file == NULL) {
    lwsl_err("Error opening file: %s", data->api_key_file_path);
    result_code = ERR_DATA_FILE_OPEN;
  } else {
    if (fputs(new_api_key, file) < 0) {
      lwsl_err("Error writing api key to: %s", data->api_key_file_path);
      result_code = ERR_DATA_FILE_WRITE;
    }

    fclose(file);
  }

  return result_code;
}
