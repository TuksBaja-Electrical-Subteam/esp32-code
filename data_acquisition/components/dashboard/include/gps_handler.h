#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void gps_init(int tx_io, int rx_io);
float get_gps_latitude(void);
float get_gps_longitude(void);

#ifdef __cplusplus
}
#endif

#endif // GPS_HANDLER_H