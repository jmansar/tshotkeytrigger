#include "config.h"
#include "datafile.h"
#include <libwebsockets.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int interrupt;
static int error_code;
static int setup_trigger;
static const char *button_id;

static int json_parse_in_progress;
static struct lejp_ctx lejp_context;
static const char *const json_paths_array[] = {"payload.apiKey"};

static struct connection_cb_ctx_t {
  lws_sorted_usec_list_t sul;
  struct lws_context *context;
} connection_cb_ctx;

static df_data_t df_data;

typedef enum { NO_MATCH, JSONPATH_PAYLOAD_API_KEY } json_paths;

typedef enum {
  MS_INIT,
  MS_SEND_AUTH,
  MS_AUTH_RESPONSE_AWAIT,
  MS_SEND_BUTTON_PRESS_DOWN,
  MS_SEND_BUTTON_PRESS_UP,
  MS_COMPLETE
} main_state_enum;

main_state_enum main_state = MS_INIT;

static void continue_setup_trigger_process() {
  printf("Navigate to Settings -> Key Bindings.\n");
  printf("Find an action that you want to bind to the trigger.\n\n");
  printf("Press ENTER key in this CLI application to continue...\n");

  getchar();

  printf("Within the next 10 seconds:\n");
  printf("\tGo back to TS settings.\n");
  printf("\tClick \"Choose\" on the selected action.\n");
  printf("\tKeep TS window focused until the trigger is successfully "
         "registered.\n");

  for (int i = 10; i > 0; i--) {
    printf("%d  ", i);
    fflush(stdout);
    sleep(1);
  }

  printf("\ntriggering: %s\n", button_id);
}

static signed char lejp_cb(struct lejp_ctx *ctx, char reason) {

  // API key should fit into internal buffer
  if (reason == LEJPCB_VAL_STR_END &&
      ctx->path_match == JSONPATH_PAYLOAD_API_KEY) {

    lwsl_notice("Received API key: %s", ctx->buf);

    df_save_api_key(&df_data, ctx->buf);

    if (setup_trigger) {
      continue_setup_trigger_process();
    }

    main_state = MS_SEND_BUTTON_PRESS_DOWN;
  }

  return 0;
}

static void build_button_press_lws_payload(const char *button_id, int is_down,
                                           char *buf, size_t buff_len) {

  lws_snprintf(&buf[LWS_PRE], buff_len, "{ \
        \"type\": \"buttonPress\", \
        \"payload\": { \
            \"state\": %s, \
            \"button\": \"%s\" \
        } \
    }",
               is_down ? "true" : "false", button_id);
}

static int tsclient_cb(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {

  uint send_buf_len = 1024;
  unsigned char send_buf[LWS_PRE + send_buf_len];
  int lejp_parse_ret_code;

  struct lws_context *context = lws_get_context(wsi);
  lwsl_cx_notice(context, "Websocket callback called %d", reason);

  if (reason == LWS_CALLBACK_CLIENT_CONNECTION_ERROR) {
    lwsl_cx_err(context, "Connection error");
    interrupt = 1;
    error_code = ERR_CONNECTION;
    lws_cancel_service(context);

    return 2;
  }

  if (reason == LWS_CALLBACK_CLIENT_ESTABLISHED) {
    lwsl_cx_user(context, "Connection established");

    main_state = MS_SEND_AUTH;

    lws_callback_on_writable(wsi);
  }

  if (reason == LWS_CALLBACK_CLIENT_WRITEABLE) {
    if (main_state == MS_SEND_AUTH) {
      lwsl_cx_user(context, "Authenticating with TS");

      if (setup_trigger) {
        printf("In TS app click on the bell icon and accept the \"HotKey CLI "
               "Trigger requests Authorization\".\n");
        printf("If the application is already authorized, you can ignore this "
               "step, exeuction will continue.\n");
      }

      memset(&send_buf, 0, sizeof send_buf);

      lws_snprintf((char *)&send_buf[LWS_PRE], send_buf_len, "{ \
        \"type\": \"auth\", \
        \"payload\": { \
            \"identifier\": \"tshotkeytrigger\", \
            \"version\": \"0\", \
            \"name\": \"HotKey CLI Trigger\", \
            \"description\": \"Triggers hotkeys via CLI\", \
            \"content\": { \
                \"apiKey\": \"%s\" \
            } \
        } \
    }",
                   df_get_api_key(&df_data));

      lws_write(wsi, &send_buf[LWS_PRE], send_buf_len, LWS_WRITE_TEXT);

      main_state = MS_AUTH_RESPONSE_AWAIT;
    } else if (main_state == MS_SEND_BUTTON_PRESS_DOWN) {

      lwsl_cx_user(context, "Send button press down");

      memset(&send_buf, 0, sizeof send_buf);
      build_button_press_lws_payload(button_id, 1, (char *)&send_buf,
                                     send_buf_len);

      lwsl_cx_notice(context, "Sending payload: %s", &send_buf[LWS_PRE]);
      lws_write(wsi, &send_buf[LWS_PRE], send_buf_len, LWS_WRITE_TEXT);

      main_state = MS_SEND_BUTTON_PRESS_UP;

      lws_callback_on_writable(wsi);
    } else if (main_state == MS_SEND_BUTTON_PRESS_UP) {

      lwsl_cx_user(context, "Send button press up");

      memset(&send_buf, 0, sizeof send_buf);
      build_button_press_lws_payload(button_id, 0, (char *)&send_buf,
                                     send_buf_len);

      lws_write(wsi, &send_buf[LWS_PRE], send_buf_len, LWS_WRITE_TEXT);

      main_state = MS_COMPLETE;
    }
  }

  if (reason == LWS_CALLBACK_CLIENT_RECEIVE) {
    lwsl_cx_notice(context, "Received: %s", (const char *)in);

    if (!json_parse_in_progress) {
      json_parse_in_progress = 1;

      lejp_construct(&lejp_context, lejp_cb, NULL, json_paths_array,
                     LWS_ARRAY_SIZE(json_paths_array));
    }

    lejp_parse_ret_code =
        lejp_parse(&lejp_context, (const unsigned char *)in, len);

    if (lejp_parse_ret_code < 0 && lejp_parse_ret_code != LEJP_CONTINUE) {
      lwsl_cx_err(context, "payload json parse error: %d", lejp_parse_ret_code);
      interrupt = 1;
      error_code = ERR_JSON_PARSE;

      json_parse_in_progress = 0;
      lejp_destruct(&lejp_context);

      return 0;
    }

    lws_callback_on_writable(wsi);
  }

  return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
    {"tsclient", tsclient_cb, 0, 0, 0, NULL, 0}, LWS_PROTOCOL_LIST_TERM};

static void connect_client(lws_sorted_usec_list_t *sul) {
  lwsl_notice("Connecting");

  struct connection_cb_ctx_t *cb_ctx =
      lws_container_of(sul, struct connection_cb_ctx_t, sul);

  struct lws_client_connect_info connect_info;
  struct lws_context *context = cb_ctx->context;

  memset(&connect_info, 0, sizeof connect_info);
  connect_info.context = context;
  connect_info.port = 5899;
  connect_info.address = "localhost";
  connect_info.host = "localhost";
  connect_info.origin = connect_info.address;
  connect_info.protocol = protocols[0].name;

  if (!lws_client_connect_via_info(&connect_info)) {
    lwsl_cx_err(context, "Error while initializing connection");

    interrupt = 1;
    error_code = ERR_CONNECTION_INIT;

    // interrupt event loop immediately
    lws_cancel_service(context);
  }
}

static void sigint_handler(int sig) { interrupt = 1; }

int main(int argc, const char **argv) {
  error_t result_code;
  int event_loop_result = 0;
  signal(SIGINT, sigint_handler);

  if (lws_cmdline_option(argc, argv, "-h") ||
      lws_cmdline_option(argc, argv, "--help")) {
    printf("tshotkeytrigger %s\n", VERSION_STR);
    printf("Usage: tshotkeytrigger [OPTIONS]\n\n");
    printf("Trigger a hotkey action in the TS 6 app using the Remote "
           "Apps API.\n");
    printf("Example: tshotkeytrigger --button-id \"toggle.mute\"\n");
    printf("<button_id> - button identifier chosen by you. It can be any "
           "string.\n\n");
    printf("Options:\n");
    printf("\t--button-id                ID of the virtual key to trigger. "
           "e.g. \"toggle.mute\"\n");
    printf("\t-s, --setup                interactive setup mode to configure a "
           "new hotkey in TS\n");
    printf("\t                             example: --setup --button-id "
           "<button_id>\n");
    printf("\t-v, --verbose              enable additional logging\n");
    printf("\t-h, --help                 display the help and exit\n");

    return 0;
  }

  if (lws_cmdline_option(argc, argv, "--verbose") ||
      lws_cmdline_option(argc, argv, "-v")) {
    lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, NULL);
  } else {
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
  }

  if (!(button_id = lws_cmdline_option(argc, argv, "--button-id"))) {
    printf("Invalid parameters.\n");
    printf("Use: \"tshotkeytrigger --help\" to see the correct usage.");

    return ERR_PARAMETERS;
  }

  if (lws_cmdline_option(argc, argv, "--setup") ||
      lws_cmdline_option(argc, argv, "-s")) {
    setup_trigger = 1;

    printf("=======================================================\n");
    printf("              Setting up a new trigger.\n");
    printf("=======================================================\n\n");

    printf("Open TS application then press ENTER key in this CLI application "
           "to continue...\n");

    getchar();
  }

  lwsl_notice("Initializing");

  result_code = df_init(&df_data);
  if (result_code != 0) {
    return result_code;
  }

  struct lws_context *context;
  struct lws_context_creation_info creation_info;

  memset(&creation_info, 0, sizeof creation_info);
  creation_info.port = CONTEXT_PORT_NO_LISTEN;
  creation_info.protocols = protocols;
  creation_info.fd_limit_per_thread = 1 + 1 + 1;

  context = lws_create_context(&creation_info);

  if (!context) {
    lwsl_err(">LWS init failed");
    return ERR_LWS_INIT;
  }

  memset(&connection_cb_ctx, 0, sizeof(connection_cb_ctx));
  connection_cb_ctx.context = context;
  lws_sul_schedule(context, 0, &connection_cb_ctx.sul, connect_client, 1);

  while (event_loop_result >= 0 && !interrupt && main_state != MS_COMPLETE)
    event_loop_result = lws_service(context, 0);

  lws_context_destroy(context);

  return error_code;
}
