#include "error.h"
#include <stddef.h>

#define PATH_BUF_SIZE 4096
#define API_KEY_BUF_SIZE 40

typedef struct {
  char api_key_file_path[PATH_BUF_SIZE];
  char api_key[API_KEY_BUF_SIZE];
} df_data_t;

error_t df_init(df_data_t *data);

const char *df_get_api_key(df_data_t *data);

error_t df_save_api_key(df_data_t *data, const char *new_api_key);
