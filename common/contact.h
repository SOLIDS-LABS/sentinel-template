/* common/contact.h — the one record format all four parts share.
 *
 * A contact report is one line of text, at most CONTACT_LINE_MAX bytes
 * including the newline:
 *
 *   <time> <sensor> <contact> <lat> <lon> <speed> <flag>
 *   1755534061.482913 NWS ACCF-4587 68.7100 -105.7000 420.0 P
 *
 * The 128-byte limit is a design decision, not a detail. Linux writes up to
 * PIPE_BUF (4096) bytes to a pipe in one atomic step, so four sensor
 * processes can share one pipe and their lines never interleave. Lab 2
 * measures this. Raising the limit past PIPE_BUF would break Part 2.
 */

#ifndef SENTINEL_CONTACT_H
#define SENTINEL_CONTACT_H

#include <stdint.h>
#include <stddef.h>

/* Including the '\n'. A buffer holding a line needs this plus 1 for '\0'. */
#define CONTACT_LINE_MAX 128

#define CONTACT_SENSOR_LEN 3    /* exactly 3 letters: NWS AUR AIS SAT */
#define CONTACT_ID_MAX     15   /* up to 15 characters */

/* The four sensors of Table 1, and their nominal rates. */
typedef enum {
    SENSOR_NWS = 0,   /* North Warning System radar,   ~20/s   */
    SENSOR_AUR,       /* CP-140 Aurora patrol aircraft, ~5/s   */
    SENSOR_AIS,       /* Coastal radar and AIS receiver,~50/s  */
    SENSOR_SAT,       /* RADARSAT satellite,        ~2000/burst */
    SENSOR_COUNT
} sensor_id_t;

typedef enum {
    PRIORITY_ROUTINE = 0,   /* 'R' */
    PRIORITY_PRIORITY = 1   /* 'P' */
} priority_t;

typedef struct {
    int64_t  t_sec;                            /* seconds since the epoch    */
    int32_t  t_usec;                           /* microseconds, 0..999999    */
    char     sensor[CONTACT_SENSOR_LEN + 1];   /* NUL-terminated             */
    char     id[CONTACT_ID_MAX + 1];           /* NUL-terminated             */
    double   lat;                              /* degrees, '-' is south      */
    double   lon;                              /* degrees, '-' is west       */
    double   speed;                            /* knots                      */
    priority_t flag;
} contact_t;

/* Why the parser rejects a line. contact_parse returns one of these as it
   is, so a caller can report exactly which rule the input broke, and
   contact_strerror names it. */
typedef enum {
    CONTACT_OK = 0,
    CONTACT_ERR_TOO_LONG,      /* line is empty, or exceeds CONTACT_LINE_MAX */
    CONTACT_ERR_FIELD_COUNT,   /* not exactly 7 fields                   */
    CONTACT_ERR_TIME,          /* malformed <sec>.<usec>                 */
    CONTACT_ERR_SENSOR,        /* not one of NWS AUR AIS SAT             */
    CONTACT_ERR_ID,            /* empty, or longer than CONTACT_ID_MAX   */
    CONTACT_ERR_LAT,           /* not a number, or outside -90..90       */
    CONTACT_ERR_LON,           /* not a number, or outside -180..180     */
    CONTACT_ERR_SPEED,         /* not a number, or negative              */
    CONTACT_ERR_FLAG           /* not 'P' or 'R'                         */
} contact_err_t;

/* Human-readable name for a contact_err_t. Never returns NULL. */
const char *contact_strerror(contact_err_t e);

/* Name of a sensor ("NWS"), or NULL if id is out of range. */
const char *sensor_name(sensor_id_t id);

/* Nominal reports per second for a sensor, from Table 1. SAT reports in a
   burst rather than a steady rate, so its value is the burst size. */
int sensor_rate(sensor_id_t id);

/* Format c into buf as one line, including the trailing '\n'.
 * Returns the number of bytes written, or -1 if the result would not fit in
 * buflen or would exceed CONTACT_LINE_MAX.
 */
int contact_format(const contact_t *c, char *buf, size_t buflen);

/* Parse one line into out. The line may end with '\n' or not; len is the
 * length excluding any '\0'.
 *
 * Returns CONTACT_OK (0) on success, or one of the contact_err_t values.
 * out is only modified on success.
 */
contact_err_t contact_parse(const char *line, size_t len, contact_t *out);

#endif /* SENTINEL_CONTACT_H */
