/*--------------------------------------------------------------------
 * mbsys_gsfnative.c  --  VALIDATED v4
 *
 * Data-system functions (allocate/deallocate/extract/insert/etc.)
 * for the native GSF v03.11 reader, data system "gsfnative".
 *
 * Companion to mbsys_gsfnative.h and mbr_gsfnativ.c.
 *
 * VALIDATION CHANGELOG (v3 -> v4), each item confirmed against real
 * MB-System source supplied by the user (see mbsys_gsfnative.h header
 * comment for the exact file list):
 *   1. mbsys_gsfnative_pingnumber(): signature changed from
 *      `int *pingnumber` to `unsigned int *pingnumber` to match
 *      struct mb_io_struct's mb_io_pingnumber function-pointer slot
 *      exactly, as defined in mb_io.h.
 *   2. mbsys_gsfnative_extract_altitude(): replaced the nonexistent
 *      error code MB_ERROR_VALUE_OUT_OF_RANGE with MB_ERROR_MISSING_DATA
 *      (value 16), which is the real code defined in mb_status.h and
 *      is the code MB-System's own modules use for "could not compute
 *      a derived quantity because no usable input was present".
 *   3. Beam-flag encoding changed from `MB_FLAG_FLAG + MB_FLAG_MANUAL`
 *      to `MB_FLAG_FLAG | MB_FLAG_MANUAL` -- functionally identical
 *      given the confirmed values (0x01 + 0x04 == 0x01 | 0x04 == 0x05,
 *      matching mb_status.h's own mb_beam_set_flag_manual() macro
 *      definition), but OR is the idiomatically correct and safe form
 *      for combining independent bit flags.
 *   4. sonartype value MB_TOPOGRAPHY_TYPE_UNKNOWN: CONFIRMED to exist
 *      exactly as used, in mb_status.h. No change needed.
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "mb_status.h"
#include "mb_format.h"
#include "mb_define.h"
#include "mb_io.h"
#include "mbsys_gsfnative.h"

static const char rcs_id[] = "$Id: mbsys_gsfnative.c, native GSF v03.11 reader, validated v4 $";

/*--------------------------------------------------------------------*/
/* Scale-factor decode/encode helpers */
/*--------------------------------------------------------------------*/

double mbsys_gsfnative_decode_scaled(int raw_value, float multiplier, float offset) {
    /* GSFlib Docs: the offset exists specifically to let negative
     * real-world values be represented by an UNSIGNED stored integer,
     * which is only consistent with subtracting the offset AFTER
     * dividing by the multiplier. */
    return ((double)raw_value / (double)multiplier) - (double)offset;
}

int mbsys_gsfnative_encode_scaled(double real_value, float multiplier, float offset) {
    return (int)((real_value + (double)offset) * (double)multiplier);
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_alloc(int verbose, void *mbio_ptr, void **store_ptr, int *error) {
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:  %d\n", verbose);
        fprintf(stderr, "dbg2  mbio_ptr: %p\n", (void *)mbio_ptr);
    }

    status = mb_mallocd(verbose, __FILE__, __LINE__,
                         sizeof(struct mbsys_gsfntv_struct), (void **)store_ptr, error);

    if (status == MB_SUCCESS)
        memset(*store_ptr, 0, sizeof(struct mbsys_gsfntv_struct));

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  store_ptr: %p\n", (void *)*store_ptr);
        fprintf(stderr, "dbg2  error:     %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status:    %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_deall(int verbose, void *mbio_ptr, void **store_ptr, int *error) {
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:   %d\n", verbose);
        fprintf(stderr, "dbg2  store_ptr: %p\n", (void *)*store_ptr);
    }

    status = mb_freed(verbose, __FILE__, __LINE__, (void **)store_ptr, error);

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_dimensions(int verbose, void *mbio_ptr, void *store_ptr,
                                int *kind, int *nbath, int *namp, int *nss, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    int status = MB_SUCCESS;

    *kind = store->kind;
    if (*kind == MB_DATA_DATA) {
        *nbath = store->ping.header.num_beams;
        *namp = *nbath; /* GSF mean-calibrated-amplitude array shares the depth-array beam indexing */
        *nss = 0;        /* no sidescan in this module */
    } else {
        *nbath = 0;
        *namp = 0;
        *nss = 0;
    }

    *error = MB_ERROR_NO_ERROR;
    return status;
}

/*--------------------------------------------------------------------*/
/* FIX 1: signature changed to unsigned int *pingnumber to match
 * struct mb_io_struct's mb_io_pingnumber slot exactly (mb_io.h). */
int mbsys_gsfnative_pingnumber(int verbose, void *mbio_ptr, unsigned int *pingnumber, int *error) {
    /* GSF's ping header does not carry an explicit sequential ping-number
     * field in the subset decoded by this module; left at zero. A real
     * ping-number field, if needed, would come from a Processing
     * Parameters or Sensor Parameters record, outside this module's
     * v1 scope. */
    *pingnumber = 0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_sonartype(int verbose, void *mbio_ptr, void *store_ptr,
                               int *sonartype, int *error) {
    /* Sensor identity is not decoded by this module's v1 scope (it would
     * come from a sensor-specific ping subrecord or a Processing/Sensor
     * Parameter record). Report "unknown" rather than guessing.
     * CONFIRMED: MB_TOPOGRAPHY_TYPE_UNKNOWN (value 0) exists in mb_status.h. */
    *sonartype = MB_TOPOGRAPHY_TYPE_UNKNOWN;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_extract(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int time_i[7], double *time_d,
                             double *navlon, double *navlat, double *speed, double *heading,
                             int *nbath, int *namp, int *nss,
                             char *beamflag, double *bath, double *amp,
                             double *bathacrosstrack, double *bathalongtrack,
                             double *ss, double *ssacrosstrack, double *ssalongtrack,
                             char *comment, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
    }

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;
    *nss = 0;

    if (*kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = ping->navlon;
        *navlat = ping->navlat;
        *heading = ping->heading;
        *speed = ping->speed * 1.852; /* knots -> km/hr, MB-System standard speed unit */

        /* Spec Appendix C.1: bit 0 of PINGFLAGS set -> whole ping unusable */
        if (ping->header.ping_flags & MBSYS_GSFNTV_PINGFLAG_IGNORE) {
            *nbath = 0;
            *namp = 0;
            if (verbose >= 2)
                fprintf(stderr, "dbg2  Ping flagged entirely unusable (PINGFLAGS bit 0 set)\n");
        } else {
            *nbath = ping->header.num_beams;
            *namp = *nbath;

            if (*nbath > MBSYS_GSFNTV_MAX_BEAMS)
                *nbath = MBSYS_GSFNTV_MAX_BEAMS; /* defensive clamp; should never trigger */

            for (i = 0; i < *nbath; i++) {
                bath[i] = ping->depth[i];
                bathacrosstrack[i] = ping->across_track[i];
                bathalongtrack[i] = ping->along_track[i];
                amp[i] = ping->amplitude[i];

                /* Spec Appendix C.2: bit 0 of the per-beam flag = ignore/invalid.
                 * FIX 3: OR instead of + for combining independent bit flags. */
                if (ping->beam_flags[i] & MBSYS_GSFNTV_BEAMFLAG_IGNORE)
                    beamflag[i] = MB_FLAG_FLAG | MB_FLAG_MANUAL;
                else
                    beamflag[i] = MB_FLAG_NONE;
            }
        }
    } else if (*kind == MB_DATA_COMMENT) {
        /* Comment record (GSF record id 6) is outside this module's v1 scope. */
        if (comment != NULL)
            comment[0] = '\0';
        *nbath = 0;
        *namp = 0;
    } else {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *nbath = 0;
        *namp = 0;
    }

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  kind:   %d\n", *kind);
        fprintf(stderr, "dbg2  nbath:  %d\n", *nbath);
        fprintf(stderr, "dbg2  error:  %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_insert(int verbose, void *mbio_ptr, void *store_ptr,
                            int kind, int time_i[7], double time_d,
                            double navlon, double navlat, double speed, double heading,
                            int nbath, int namp, int nss,
                            char *beamflag, double *bath, double *amp,
                            double *bathacrosstrack, double *bathalongtrack,
                            double *ss, double *ssacrosstrack, double *ssalongtrack,
                            char *comment, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    store->kind = kind;

    if (kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) store->time_i[i] = time_i[i];
        store->time_d = time_d;

        ping->navlon = navlon;
        ping->navlat = navlat;
        ping->heading = heading;
        ping->speed = speed / 1.852; /* km/hr -> knots, reverse of extract() */

        ping->header.num_beams = (short)(nbath < MBSYS_GSFNTV_MAX_BEAMS ? nbath : MBSYS_GSFNTV_MAX_BEAMS);

        for (i = 0; i < ping->header.num_beams; i++) {
            ping->depth[i] = bath[i];
            ping->across_track[i] = bathacrosstrack[i];
            ping->along_track[i] = bathalongtrack[i];
            ping->amplitude[i] = (i < namp) ? amp[i] : 0.0;

            if (beamflag[i] == MB_FLAG_NONE)
                ping->beam_flags[i] = 0;
            else
                ping->beam_flags[i] = MBSYS_GSFNTV_BEAMFLAG_IGNORE;
        }
    } else if (kind == MB_DATA_COMMENT) {
        /* comment storage intentionally omitted -- out of v1 scope */
    }

    *error = MB_ERROR_NO_ERROR;
    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_extract_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                 int *kind, int time_i[7], double *time_d,
                                 double *navlon, double *navlat, double *speed, double *heading,
                                 double *draft, double *roll, double *pitch, double *heave, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;

    if (*kind == MB_DATA_DATA) {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = ping->navlon;
        *navlat = ping->navlat;
        *heading = ping->heading;
        *speed = ping->speed * 1.852;
        *draft = (double)ping->header.depth_corrector / 100.0; /* centimeters -> meters */

        /* Ping header's own roll/pitch/heave. If the calling program has
         * already accumulated Attitude-record samples via mb_attint_add()
         * and prefers the independently time-stamped, potentially
         * higher-rate series, it should call mb_attint_interp() itself
         * rather than rely on these ping-header values -- this function
         * intentionally does not override them here. */
        *roll = (double)ping->header.roll_raw / 100.0;
        *pitch = (double)ping->header.pitch_raw / 100.0;
        *heave = (double)ping->header.heave / 100.0;
    } else {
        for (i = 0; i < 7; i++) time_i[i] = store->time_i[i];
        *time_d = store->time_d;
        *navlon = 0.0;
        *navlat = 0.0;
        *speed = 0.0;
        *heading = 0.0;
        *draft = 0.0;
        *roll = 0.0;
        *pitch = 0.0;
        *heave = 0.0;
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_insert_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                int time_i[7], double time_d,
                                double navlon, double navlat, double speed, double heading,
                                double draft, double roll, double pitch, double heave, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int i;

    for (i = 0; i < 7; i++) store->time_i[i] = time_i[i];
    store->time_d = time_d;

    ping->navlon = navlon;
    ping->navlat = navlat;
    ping->heading = heading;
    ping->speed = speed / 1.852;

    ping->header.depth_corrector = (int)(draft * 100.0);
    ping->header.roll_raw = (short)(roll * 100.0);
    ping->header.pitch_raw = (short)(pitch * 100.0);
    ping->header.heave = (short)(heave * 100.0);

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* FIX 2: replaced nonexistent MB_ERROR_VALUE_OUT_OF_RANGE with the
 * real, confirmed-to-exist MB_ERROR_MISSING_DATA (mb_status.h). */
int mbsys_gsfnative_extract_altitude(int verbose, void *mbio_ptr, void *store_ptr,
                                      int *kind, double *transducer_depth, double *altitude, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int status = MB_SUCCESS;
    int i;
    double shallowest = 0.0;
    int found_valid = 0;

    *kind = store->kind;
    *error = MB_ERROR_NO_ERROR;

    if (*kind == MB_DATA_DATA) {
        *transducer_depth = (double)ping->header.depth_corrector / 100.0;

        /* Altitude above bottom approximated as the shallowest usable
         * beam's depth minus the transducer depth. A production-quality
         * implementation would instead prefer the nadir (center) beam
         * specifically (ping->header.center_beam) when it is usable;
         * this simpler approach is adequate for a v1 module. */
        for (i = 0; i < ping->header.num_beams; i++) {
            if (!(ping->beam_flags[i] & MBSYS_GSFNTV_BEAMFLAG_IGNORE)) {
                if (!found_valid || ping->depth[i] < shallowest) {
                    shallowest = ping->depth[i];
                    found_valid = 1;
                }
            }
        }

        if (found_valid) {
            *altitude = shallowest - *transducer_depth;
        } else {
            *altitude = 0.0;
            *error = MB_ERROR_MISSING_DATA;
            status = MB_FAILURE;
        }
    } else {
        *transducer_depth = 0.0;
        *altitude = 0.0;
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_ttimes(int verbose, void *mbio_ptr, void *store_ptr,
                            int *kind, int *nbeams,
                            double *ttime, double *angle_xtrack, double *angle_ltrack,
                            double *heave, double *alongtrack_offset,
                            double *draft, double *ssv, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;

    /* This module's v1 scope does not decode TRAVELTIMEARRAY (subrecord id 4)
     * or BEAMANGLEARRAY (subrecord id 5) because it stores pre-computed
     * DEPTHARRAY/ACROSSTRACKARRAY/ALONGTRACKARRAY values directly
     * (mbr_info_gsfnativ() sets *traveltime = false accordingly).
     * Raytraced-bathymetry recalculation is therefore not supported;
     * this function reports zero beams rather than fabricating values. */
    *kind = store->kind;
    *nbeams = 0;
    *draft = (double)store->ping.header.depth_corrector / 100.0;
    *ssv = 0.0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_detects(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int *nbeams, int *detects, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    int i;

    /* Bottom-detection method (amplitude vs. phase) is carried in GSF's
     * DETECTIONINFOARRAY subrecord (id 23), outside this module's v1
     * scope. Report as unknown for every beam rather than guessing. */
    *kind = store->kind;
    if (*kind == MB_DATA_DATA) {
        *nbeams = ping->header.num_beams;
        for (i = 0; i < *nbeams && i < MBSYS_GSFNTV_MAX_BEAMS; i++)
            detects[i] = MB_DETECT_UNKNOWN;
    } else {
        *nbeams = 0;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_gains(int verbose, void *mbio_ptr, void *store_ptr,
                           int *kind, double *transmit_gain, double *pulse_length,
                           double *receive_gain, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;

    /* Transmit/receive gain and pulse length are carried in sensor-specific
     * ping subrecords, outside this module's v1 scope. Report zero/unknown
     * rather than guessing. */
    *kind = store->kind;
    *transmit_gain = 0.0;
    *pulse_length = 0.0;
    *receive_gain = 0.0;
    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
int mbsys_gsfnative_copy(int verbose, void *mbio_ptr,
                          void *store_ptr, void *copy_ptr, int *error) {
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    struct mbsys_gsfntv_struct *copy = (struct mbsys_gsfntv_struct *)copy_ptr;

    *copy = *store; /* flat struct, no internal pointers -- a shallow copy is exact and sufficient */

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}
