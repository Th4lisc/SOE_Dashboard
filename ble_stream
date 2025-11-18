// ble_obd_stream.c
// Leitura BLE (gatttool) + parser OBD-II ascii-hex + WebSocket JSON (por PID)
// Compile: gcc -o ble_obd_stream ble_obd_stream.c -lwebsockets -lpthread
// Run: sudo ./ble_obd_stream AA:BB:CC:DD:EE:FF

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <libwebsockets.h>

#define WS_PORT 9090

// Handles (conforme informado)
#define NOTIFY_VALUE_HANDLE "0x0015" // descriptor to enable notify
#define WRITE_HANDLE "0x0017"        // handle to write commands

// global runtime control
static int running = 1;
static struct lws_context *ws_context = NULL;
static const struct lws_protocols protocols[]; // forward

// Pending JSON message (single slot, protected by mutex)
static char pending_msg[512];
static int pending_flag = 0;
static pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;

// MAC address from argv
static char ble_mac[64];

// PIDs to poll (as strings)
static const char *pids[] = { "010C", "010D", "0111", "010B", "0105", "0142" };
static const int n_pids = sizeof(pids) / sizeof(pids[0]);

// helper: hex-encode ascii string (e.g. "010C\r\n" -> "303130430d0a")
static void ascii_to_hexstr_crlf(const char *ascii, char *out_hex, size_t out_len) {
    size_t p = 0;
    for (size_t i = 0; ascii[i] != '\0' && p + 2 < out_len; ++i) {
        unsigned char c = (unsigned char)ascii[i];
        int hi = (c >> 4) & 0xF;
        int lo = c & 0xF;
        if (p + 2 >= out_len) break;
        out_hex[p++] = "0123456789ABCDEF"[hi];
        out_hex[p++] = "0123456789ABCDEF"[lo];
    }
    out_hex[p] = '\0';
}

// helper: build command hex with CRLF termination
static void build_cmd_hex(const char *pid_ascii, char *out_hex, size_t out_len) {
    // build ascii with CRLF appended
    char ascii_cmd[32];
    snprintf(ascii_cmd, sizeof(ascii_cmd), "%s\r\n", pid_ascii);
    ascii_to_hexstr_crlf(ascii_cmd, out_hex, out_len);
}

// send char-write-req via gatttool (blocking)
static int send_gatt_write(const char *handle, const char *hexpayload) {
    char cmd[512];
    // use --retry to be a bit more robust (some gatttool versions accept --retry; if not, remove)
    snprintf(cmd, sizeof(cmd),
             "gatttool -b %s --char-write-req -a %s -n %s 2>&1",
             ble_mac, handle, hexpayload);
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    // read output for debugging if needed
    char buf[256];
    int rc = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        // optionally log
        // fprintf(stderr, "[gatt-wr] %s", buf);
        if (strstr(buf, "Error") || strstr(buf, "Failed")) rc = -1;
    }
    pclose(fp);
    return rc;
}

// enable notify by writing 0100 to descriptor handle
static int enable_notify() {
    return send_gatt_write(NOTIFY_VALUE_HANDLE, "0100");
}

// thread: writer -> polls PIDs periodically and writes them (CRLF terminated)
static void *writer_thread(void *arg) {
    (void)arg;
    while (running) {
        for (int i = 0; i < n_pids && running; ++i) {
            char hexpayload[128];
            build_cmd_hex(pids[i], hexpayload, sizeof(hexpayload));
            // send to write handle
            int rc = send_gatt_write(WRITE_HANDLE, hexpayload);
            if (rc != 0) {
                // try reconnect briefly: a simple ping connect via char-write to notify descriptor
                enable_notify();
            }
            // small pause between writes - adapt to reduce bus saturation
            usleep(200000); // 200 ms between commands
        }
        // small pause between cycles
        usleep(200000); // additional 200 ms -> ~ (n_pids*0.2 + 0.2) seconds per cycle
    }
    return NULL;
}

// parse hex tokens string (e.g. "41 0C 0C FB") and produce JSON per PID
static void process_obd_tokens(const unsigned char *bytes, int count) {
    // bytes contain token bytes (not ASCII hex, but parsed hex bytes like 0x41, 0x0C, ...)
    // iterate and find 0x41 markers
    for (int i = 0; i < count; ++i) {
        if (bytes[i] != 0x41) continue;
        if (i + 1 >= count) continue;
        unsigned int pid = bytes[i+1];
        char json[256];
        if (pid == 0x0C && i + 3 < count) {
            // RPM
            int A = bytes[i+2];
            int B = bytes[i+3];
            int rpm = ((A * 256) + B) / 4;
            snprintf(json, sizeof(json), "{\"rpm\": %d}", rpm);
        } else if (pid == 0x0D && i + 2 < count) {
            int A = bytes[i+2];
            snprintf(json, sizeof(json), "{\"speed\": %d}", A);
        } else if (pid == 0x11 && i + 2 < count) {
            int A = bytes[i+2];
            double tps = (A * 100.0) / 255.0;
            snprintf(json, sizeof(json), "{\"tps\": %.1f}", tps);
        } else if (pid == 0x0B && i + 2 < count) {
            int A = bytes[i+2];
            snprintf(json, sizeof(json), "{\"map\": %d}", A);
        } else if (pid == 0x05 && i + 2 < count) {
            int A = bytes[i+2];
            int temp = A - 40;
            snprintf(json, sizeof(json), "{\"coolant\": %d}", temp);
        } else if (pid == 0x42 && i + 3 < count) {
            int A = bytes[i+2];
            int B = bytes[i+3];
            double battery = ((A * 256) + B) / 1000.0;
            snprintf(json, sizeof(json), "{\"battery\": %.3f}", battery);
        } else {
            continue;
        }

        // publish pending message (thread-safe)
        pthread_mutex_lock(&pending_mutex);
        strncpy(pending_msg, json, sizeof(pending_msg)-1);
        pending_msg[sizeof(pending_msg)-1] = '\0';
        pending_flag = 1;
        pthread_mutex_unlock(&pending_mutex);

        // notify websocket context to become writable
        if (ws_context) {
            // use the first protocol
            extern const struct lws_protocols protocols[];
            lws_callback_on_writable_all_protocol(ws_context, &protocols[0]);
        }
    }
}

// helper: parse ascii hex tokens string into bytes
// Input example: "Notification handle = 0x0015 value: 34 31 20 30 43 20 30 43 20 46 42 20 0d 0a 0d 0a 3e"
static void parse_notification_line(const char *line) {
    const char *p = strstr(line, "value:");
    if (!p) return;
    p += strlen("value:");
    // read hex pairs
    unsigned char bytes[256];
    int bcount = 0;
    while (*p && bcount < (int)sizeof(bytes)) {
        // skip spaces
        while (*p && isspace((unsigned char)*p)) p++;
        if (!isxdigit((unsigned char)p[0])) break;
        // parse two hex chars
        unsigned int v;
        if (sscanf(p, "%2x", &v) != 1) break;
        bytes[bcount++] = (unsigned char)v;
        // move forward past two hex chars
        // advance until next non-hex or space
        int adv = 0;
        while (p[adv] && !isspace((unsigned char)p[adv])) adv++;
        p += adv;
    }

    if (bcount == 0) return;

    // Convert bytes representing ASCII-hex into raw bytes OR parse ASCII content
    // The device sends ASCII hex characters representing the response, terminated by CRLF CRLF and '>' prompt.
    // So we first convert bytes -> ASCII string, then extract tokens like "41 0C 0C FB"
    char ascii[1024]; int ai = 0;
    for (int i = 0; i < bcount && ai + 1 < (int)sizeof(ascii); ++i) {
        ascii[ai++] = (char)bytes[i];
    }
    ascii[ai] = '\0';

    // Find '>' and truncate at it
    char *gt = strchr(ascii, '>');
    if (gt) *gt = '\0';

    // Now ascii contains something like "41 0C 0C FB \r\n\r\n" or maybe "41 0C 0C FB"
    // Remove CR/LF
    char cleaned[1024]; int ci = 0;
    for (int i = 0; i < ai && ci + 1 < (int)sizeof(cleaned); ++i) {
        if (ascii[i] == '\r' || ascii[i] == '\n') continue;
        cleaned[ci++] = ascii[i];
    }
    cleaned[ci] = '\0';

    // Now cleaned contains "41 0C 0C FB " possibly with extra spaces. Tokenize hex pairs and convert to bytes.
    unsigned char token_bytes[256]; int tcount = 0;
    char *tok = strtok(cleaned, " ");
    while (tok && tcount < (int)sizeof(token_bytes)) {
        unsigned int v;
        if (sscanf(tok, "%x", &v) == 1) {
            token_bytes[tcount++] = (unsigned char)v;
        }
        tok = strtok(NULL, " ");
    }

    if (tcount > 0) {
        process_obd_tokens(token_bytes, tcount);
    }
}

// thread: listener -> gatttool --listen
static void *listener_thread(void *arg) {
    (void)arg;
    // start listen process (it will create a connection and wait for notifications)
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "gatttool -b %s --listen 2>&1", ble_mac);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "Failed to start gatttool --listen\n");
        running = 0;
        return NULL;
    }
    char line[512];
    while (running && fgets(line, sizeof(line), fp)) {
        // typical notif line: "Notification handle = 0x0015 value: 34 31 20 30 43 ..."
        if (strstr(line, "Notification handle")) {
            parse_notification_line(line);
        }
    }
    pclose(fp);
    return NULL;
}

// WebSocket callback (protocol)
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    (void)user; (void)in; (void)len;
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            lwsl_notice("Client connected\n");
            break;
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            // if pending message available, send it
            pthread_mutex_lock(&pending_mutex);
            int have = pending_flag;
            char msg[512];
            if (have) {
                strncpy(msg, pending_msg, sizeof(msg)-1);
                msg[sizeof(msg)-1] = '\0';
                pending_flag = 0;
            }
            pthread_mutex_unlock(&pending_mutex);

            if (have) {
                // send as text
                unsigned char buf[LWS_PRE + 512];
                size_t m = strlen(msg);
                memcpy(&buf[LWS_PRE], msg, m);
                lws_write(wsi, &buf[LWS_PRE], m, LWS_WRITE_TEXT);
            }
            break;
        }
        case LWS_CALLBACK_CLOSED:
            lwsl_notice("Client disconnected\n");
            break;
        default:
            break;
    }
    return 0;
}

static const struct lws_protocols protocols_local[] = {
    { "obd-protocol", ws_callback, 0, 1024 },
    { NULL, NULL, 0, 0 }
};
const struct lws_protocols protocols[] = {
    { "obd-protocol", ws_callback, 0, 1024 },
    { NULL, NULL, 0, 0 }
};

static void sigint(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sudo %s <BLE_MAC>\n", argv[0]);
        return 1;
    }
    strncpy(ble_mac, argv[1], sizeof(ble_mac)-1);
    ble_mac[sizeof(ble_mac)-1] = '\0';

    signal(SIGINT, sigint);

    // 1) Enable notify: write 0100 to descriptor (value handle)
    fprintf(stderr, "[init] Enabling notify on %s handle %s ...\n", ble_mac, NOTIFY_VALUE_HANDLE);
    if (send_gatt_write(NOTIFY_VALUE_HANDLE, "0100") != 0) {
        fprintf(stderr, "[init] Warning: could not write notify descriptor (continuing)...\n");
    }
    usleep(200000);

    // 2) Start listener thread (gatttool --listen)
    pthread_t tid_listen, tid_write;
    if (pthread_create(&tid_listen, NULL, listener_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create listener thread\n");
        return 1;
    }

    // 3) Start writer thread (poll PIDs)
    if (pthread_create(&tid_write, NULL, writer_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create writer thread\n");
        return 1;
    }

    // 4) Start WebSocket server
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = WS_PORT;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_LIBUV;
    ws_context = lws_create_context(&info);
    if (!ws_context) {
        fprintf(stderr, "lws init failed\n");
        running = 0;
    } else {
        fprintf(stderr, "[ws] WebSocket server listening on port %d\n", WS_PORT);
    }

    // 5) main loop
    while (running) {
        if (ws_context) lws_service(ws_context, 50);
        // wake all writable to push any pending messages
        if (ws_context) lws_callback_on_writable_all_protocol(ws_context, &protocols[0]);
        usleep(20000);
    }

    // cleanup
    if (ws_context) lws_context_destroy(ws_context);
    pthread_join(tid_write, NULL);
    pthread_join(tid_listen, NULL);
    return 0;
}
