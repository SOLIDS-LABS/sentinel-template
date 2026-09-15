#include "contact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

static const char *const kSensorNames[SENSOR_COUNT] = {
    "NWS", "AUR", "AIS", "SAT"
};

static const int kSensorRates[SENSOR_COUNT] = { 20, 5, 50, 2000 };

const char *sensor_name(sensor_id_t id)
{
    if ((unsigned)id >= SENSOR_COUNT) {
        return NULL;
    }
    return kSensorNames[id];
}

int sensor_rate(sensor_id_t id)
{
    if ((unsigned)id >= SENSOR_COUNT) {
        return -1;
    }
    return kSensorRates[id];
}

const char *contact_strerror(contact_err_t e)
{
    switch (e) {
    case CONTACT_OK:                return "ok";
    case CONTACT_ERR_TOO_LONG:      return "line too long";
    case CONTACT_ERR_FIELD_COUNT:   return "wrong number of fields";
    case CONTACT_ERR_TIME:          return "bad timestamp";
    case CONTACT_ERR_SENSOR:        return "unknown sensor";
    case CONTACT_ERR_ID:            return "bad contact id";
    case CONTACT_ERR_LAT:           return "bad latitude";
    case CONTACT_ERR_LON:           return "bad longitude";
    case CONTACT_ERR_SPEED:         return "bad speed";
    case CONTACT_ERR_FLAG:          return "bad priority flag";
    }
    return "unknown error";
}

int contact_format(const contact_t *c, char *buf, size_t buflen)
{
    int n = snprintf(buf, buflen, "%lld.%06d %s %s %.4f %.4f %.1f %c\n",
                     (long long)c->t_sec, (int)c->t_usec,
                     c->sensor, c->id,
                     c->lat, c->lon, c->speed,
                     c->flag == PRIORITY_PRIORITY ? 'P' : 'R');

    if (n < 0 || (size_t)n >= buflen || n > CONTACT_LINE_MAX) {
        return -1;
    }
    return n;
}

static int next_field(const char **p, const char *end, char *dst, size_t dstlen)
{
    const char *s = *p;

    while (s < end && *s == ' ') {
        s++;
    }
    const char *start = s;
    while (s < end && *s != ' ') {
        s++;
    }

    size_t len = (size_t)(s - start);
    if (len == 0 || len >= dstlen) {
        return -1;
    }
    memcpy(dst, start, len);
    dst[len] = '\0';
    *p = s;
    return 0;
}

static int parse_double(const char *s, double *out)
{
    char *endp = NULL;
    errno = 0;
    double v = strtod(s, &endp);

    if (endp == s || *endp != '\0' || errno == ERANGE || !isfinite(v)) {
        return -1;
    }
    *out = v;
    return 0;
}

contact_err_t contact_parse(const char *line, size_t len, contact_t *out)
{
    if (len > 0 && line[len - 1] == '\n') {
        len--;
    }
    if (len == 0 || len + 1 > CONTACT_LINE_MAX) {
        return CONTACT_ERR_TOO_LONG;
    }

    const char *p   = line;
    const char *end = line + len;
    contact_t   c;
    memset(&c, 0, sizeof c);

    char field[CONTACT_LINE_MAX];
    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    {
        char *dot = strchr(field, '.');
        if (dot == NULL) {
            return CONTACT_ERR_TIME;
        }
        *dot = '\0';

        char *endp = NULL;
        errno = 0;
        long long sec = strtoll(field, &endp, 10);
        if (endp == field || *endp != '\0' || errno == ERANGE || sec < 0) {
            return CONTACT_ERR_TIME;
        }

        const char *us = dot + 1;
        if (strlen(us) != 6) {
            return CONTACT_ERR_TIME;
        }
        for (int i = 0; i < 6; i++) {
            if (us[i] < '0' || us[i] > '9') {
                return CONTACT_ERR_TIME;
            }
        }
        c.t_sec  = (int64_t)sec;
        c.t_usec = (int32_t)strtol(us, NULL, 10);
    }

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    {
        int found = 0;
        for (int i = 0; i < SENSOR_COUNT; i++) {
            if (strcmp(field, kSensorNames[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            return CONTACT_ERR_SENSOR;
        }
        memcpy(c.sensor, field, CONTACT_SENSOR_LEN + 1);
    }

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    if (strlen(field) > CONTACT_ID_MAX) {
        return CONTACT_ERR_ID;
    }
    memcpy(c.id, field, strlen(field) + 1);

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    if (parse_double(field, &c.lat) != 0 || c.lat < -90.0 || c.lat > 90.0) {
        return CONTACT_ERR_LAT;
    }

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    if (parse_double(field, &c.lon) != 0 || c.lon < -180.0 || c.lon > 180.0) {
        return CONTACT_ERR_LON;
    }

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    if (parse_double(field, &c.speed) != 0 || c.speed < 0.0) {
        return CONTACT_ERR_SPEED;
    }

    if (next_field(&p, end, field, sizeof field) != 0) {
        return CONTACT_ERR_FIELD_COUNT;
    }
    if (field[1] != '\0') {
        return CONTACT_ERR_FLAG;
    }
    if (field[0] == 'P') {
        c.flag = PRIORITY_PRIORITY;
    } else if (field[0] == 'R') {
        c.flag = PRIORITY_ROUTINE;
    } else {
        return CONTACT_ERR_FLAG;
    }

    while (p < end && *p == ' ') {
        p++;
    }
    if (p != end) {
        return CONTACT_ERR_FIELD_COUNT;
    }

    *out = c;
    return CONTACT_OK;
}
