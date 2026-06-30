#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

void start_webserver(void);

#ifdef __cplusplus
}
#endif

// Session metadata structure
typedef struct {
    char id[32];
    char name[64];
    uint32_t start_time;
    uint32_t duration_ms;
    uint16_t lap_count;
    uint32_t file_size;
} session_info_t;

// Telemetry frame structure (matches WebSocket JSON)
typedef struct {
    uint32_t timestamp_ms;
    float speed_ms;
    float latitude;
    float longitude;
    float track_pct;
    uint16_t lap;
    float wheel_slip_l;
    float wheel_slip_r;
    bool session_active;
    float hdop;
    uint8_t fix_type;
} telemetry_frame_t;

// Start the web server with all endpoints
void start_webserver(void);

// Session control
bool session_start(void);
bool session_stop(void);
bool session_is_active(void);

// Telemetry broadcast (called at 10Hz from main loop)
void telemetry_broadcast(const telemetry_frame_t* frame);

// Get session list
int session_get_list(session_info_t* out_list, int max_count);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H