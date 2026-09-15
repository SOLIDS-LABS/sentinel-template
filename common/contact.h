#ifndef SENTINEL_CONTACT_H
#define SENTINEL_CONTACT_H

#include <stdint.h>
#include <stddef.h>

#define CONTACT_LINE_MAX 128

#define CONTACT_SENSOR_LEN 3
#define CONTACT_ID_MAX     15

typedef enum {
    SENSOR_NWS = 0,
    SENSOR_AUR,
    SENSOR_AIS,
    SENSOR_SAT,
    SENSOR_COUNT
} sensor_id_t;

typedef enum {
    PRIORITY_ROUTINE = 0,
    PRIORITY_PRIORITY = 1
} priority_t;

typedef struct {
    int64_t  t_sec;
    int32_t  t_usec;
    char     sensor[CONTACT_SENSOR_LEN + 1];
    char     id[CONTACT_ID_MAX + 1];
    double   lat;
    double   lon;
    double   speed;
    priority_t flag;
} contact_t;

typedef enum {
    CONTACT_OK = 0,
    CONTACT_ERR_TOO_LONG,
    CONTACT_ERR_FIELD_COUNT,
    CONTACT_ERR_TIME,
    CONTACT_ERR_SENSOR,
    CONTACT_ERR_ID,
    CONTACT_ERR_LAT,
    CONTACT_ERR_LON,
    CONTACT_ERR_SPEED,
    CONTACT_ERR_FLAG
} contact_err_t;

const char *contact_strerror(contact_err_t e);

const char *sensor_name(sensor_id_t id);

int sensor_rate(sensor_id_t id);

int contact_format(const contact_t *c, char *buf, size_t buflen);

contact_err_t contact_parse(const char *line, size_t len, contact_t *out);

#endif
