/*--------------------------------------------------------------------
 * mbr_gsfnativ.c  --  VALIDATED v4
 *
 * Format-level (byte stream <-> data structure) functions for
 * MBIO format id 300 (suggested), format name "GSFNATIV" (8 chars),
 * data system "gsfnative". A native, non-libgsf reader for the
 * Generic Sensor Format (GSF), version 03.11.
 *
 * SCOPE (v1): READ-ONLY. mbr_wt_gsfnativ() below is present (as
 * required by the mb_io_write_ping function-pointer slot) but does
 * not encode/write bytes.
 *
 * filetype = MB_FILETYPE_SINGLE: this module uses MBIO's own buffered
 * mb_fileio_open/get/put/close layer, NOT the external libgsf library
 * that format 121 uses.
 *
 * VALIDATION CHANGELOG (v3 -> v4), each confirmed against the real
 * MB-System source the user supplied:
 *   1. mbr_info_gsfnativ(): full parameter list now matches every
 *      real mbr_info_XXX() prototype in mb_format.h exactly --
 *      added platform_source and sensordepth_source parameters,
 *      renamed vru_source -> attitude_source, changed
 *      variable_beams/traveltime/beam_flagging from int* to bool*.
 *   2. mbr_register_gsfnativ(): the call into mbr_info_gsfnativ()
 *      and the struct mb_io_struct field list were updated to match
 *      (added &mb_io_ptr->platform_source, &mb_io_ptr->sensordepth_source;
 *      &mb_io_ptr->vru_source -> &mb_io_ptr->attitude_source, which is
 *      the real field name in mb_io.h).
 *   3. Every mb_get_binary_int/short/float() call now passes
 *      mb_io_ptr->byteswapped instead of a hardcoded MB_YES. Per
 *      mb_define.h, mb_get_binary_int() takes `bool swapped` as its
 *      first argument; per mb_io.h, mb_io_ptr->byteswapped is exactly
 *      that flag ("0 unswapped, 1 swapped"). Because GSF is always
 *      big-endian on disk regardless of host, this module now computes
 *      the correct value once, in mbr_alm_gsfnativ(), via the confirmed
 *      mb_swap_check() function (mb_swap.c): it returns true exactly
 *      when the host is NOT big-endian, i.e. exactly when swapping is
 *      required to read GSF's big-endian bytes correctly.
 *   4. mbr_gsfnativ_rd_data(): the previous `default: store->kind =
 *      MB_DATA_OTHER;` branch used a constant that does not exist
 *      anywhere in mb_status.h. Replaced with a full switch mapping
 *      every skipped GSF record type to the real, pre-existing
 *      MB-System kind already defined for it (several of which are
 *      explicitly documented in mb_status.h as "GSF"-sourced kinds).
 *   5. mbr_gsfnativ_rd_ping/_rd_scalefactors/_rd_beamarray/_rd_attitude
 *      now take mbio_ptr so they can read mb_io_ptr->byteswapped.
 *   6. mb_attint_add() parameter order CONFIRMED correct as originally
 *      written: (verbose, mbio_ptr, time_d, heave, roll, pitch, error),
 *      matching the prototype in mb_define.h exactly. No change needed;
 *      the previous "TODO-VERIFY" note is resolved.
 *
 * Remaining open items are GSF-Spec bit-packing questions that no
 * MB-System source file can resolve (see mbsys_gsfnative.h header);
 * they are marked TODO-VERIFY-VS-SPEC at their point of use below.
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

static const char rcs_id[] = "$Id: mbr_gsfnativ.c, native GSF v03.11 reader, validated v4 $";

/*--------------------------------------------------------------------*/
/* mbr_register_gsfnativ()
 * Loads function pointers into mb_io_struct so that generic MBIO
 * calls (mb_read_ping, mb_extract, mb_extract_nav, etc.) dispatch
 * to this module's functions when format 300 is in use. */
/*--------------------------------------------------------------------*/
int mbr_register_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> called\n", __func__);
        fprintf(stderr, "dbg2 Revision id: %s\n", rcs_id);
        fprintf(stderr, "dbg2 Input arguments:\n");
        fprintf(stderr, "dbg2  verbose:  %d\n", verbose);
        fprintf(stderr, "dbg2  mbio_ptr: %p\n", (void *)mbio_ptr);
    }

    /* FIX 1/2: full parameter list, matching the real mbr_info_XXX()
     * signature confirmed in mb_format.h (platform_source and
     * sensordepth_source added; attitude_source is the real field name). */
    status = mbr_info_gsfnativ(verbose,
                                &mb_io_ptr->system,
                                &mb_io_ptr->beams_bath_max,
                                &mb_io_ptr->beams_amp_max,
                                &mb_io_ptr->pixels_ss_max,
                                mb_io_ptr->format_name,
                                mb_io_ptr->system_name,
                                mb_io_ptr->format_description,
                                &mb_io_ptr->numfile,
                                &mb_io_ptr->filetype,
                                &mb_io_ptr->variable_beams,
                                &mb_io_ptr->traveltime,
                                &mb_io_ptr->beam_flagging,
                                &mb_io_ptr->platform_source,
                                &mb_io_ptr->nav_source,
                                &mb_io_ptr->sensordepth_source,
                                &mb_io_ptr->heading_source,
                                &mb_io_ptr->attitude_source,
                                &mb_io_ptr->svp_source,
                                &mb_io_ptr->beamwidth_xtrack,
                                &mb_io_ptr->beamwidth_ltrack,
                                error);

    /* set format and system specific function pointers -- only functions
     * that are implemented for this data system are registered; everything
     * else is left NULL so MBIO knows those capabilities are unavailable,
     * following the same convention used throughout MB-System's other
     * I/O modules (confirmed against mbr_tempform.c's mbr_register_tempform()). */
    mb_io_ptr->mb_io_format_alloc = &mbr_alm_gsfnativ;
    mb_io_ptr->mb_io_format_free = &mbr_dem_gsfnativ;
    mb_io_ptr->mb_io_store_alloc = &mbsys_gsfnative_alloc;
    mb_io_ptr->mb_io_store_free = &mbsys_gsfnative_deall;
    mb_io_ptr->mb_io_read_ping = &mbr_rt_gsfnativ;
    mb_io_ptr->mb_io_write_ping = &mbr_wt_gsfnativ;
    mb_io_ptr->mb_io_dimensions = &mbsys_gsfnative_dimensions;
    mb_io_ptr->mb_io_pingnumber = &mbsys_gsfnative_pingnumber;
    mb_io_ptr->mb_io_sonartype = &mbsys_gsfnative_sonartype;
    mb_io_ptr->mb_io_sidescantype = NULL; /* no sidescan support in this module */
    mb_io_ptr->mb_io_extract = &mbsys_gsfnative_extract;
    mb_io_ptr->mb_io_insert = &mbsys_gsfnative_insert;
    mb_io_ptr->mb_io_extract_nav = &mbsys_gsfnative_extract_nav;
    mb_io_ptr->mb_io_extract_nnav = NULL;
    mb_io_ptr->mb_io_insert_nav = &mbsys_gsfnative_insert_nav;
    mb_io_ptr->mb_io_extract_altitude = &mbsys_gsfnative_extract_altitude;
    mb_io_ptr->mb_io_insert_altitude = NULL;
    mb_io_ptr->mb_io_extract_svp = NULL; /* Sound Velocity Profile record not in v1 scope */
    mb_io_ptr->mb_io_insert_svp = NULL;
    mb_io_ptr->mb_io_ttimes = &mbsys_gsfnative_ttimes;
    mb_io_ptr->mb_io_detects = &mbsys_gsfnative_detects;
    mb_io_ptr->mb_io_gains = &mbsys_gsfnative_gains;
    mb_io_ptr->mb_io_copyrecord = &mbsys_gsfnative_copy;
    mb_io_ptr->mb_io_extract_rawss = NULL;
    mb_io_ptr->mb_io_insert_rawss = NULL;
    mb_io_ptr->mb_io_extract_segytraceheader = NULL;
    mb_io_ptr->mb_io_extract_segy = NULL;
    mb_io_ptr->mb_io_insert_segy = NULL;
    mb_io_ptr->mb_io_ctd = NULL;
    mb_io_ptr->mb_io_ancilliarysensor = NULL;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return values:\n");
        fprintf(stderr, "dbg2  system: %d\n", mb_io_ptr->system);
        fprintf(stderr, "dbg2  error:  %d\n", *error);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_info_gsfnativ()
 * Sets format parameters and modes.
 * FIX 1: signature now exactly matches every real mbr_info_XXX() in
 * mb_format.h -- added platform_source/sensordepth_source parameters,
 * bool* for variable_beams/traveltime/beam_flagging, attitude_source
 * instead of vru_source. */
/*--------------------------------------------------------------------*/
int mbr_info_gsfnativ(int verbose,
                       int *system, int *beams_bath_max, int *beams_amp_max, int *pixels_ss_max,
                       char *format_name, char *system_name, char *format_description,
                       int *numfile, int *filetype,
                       bool *variable_beams, bool *traveltime, bool *beam_flagging,
                       int *platform_source, int *nav_source, int *sensordepth_source,
                       int *heading_source, int *attitude_source, int *svp_source,
                       double *beamwidth_xtrack, double *beamwidth_ltrack, int *error) {
    int status = MB_SUCCESS;

    *error = MB_ERROR_NO_ERROR;

    /* MB_SYS_GSFNATIVE must be defined in mb_format.h. Based on the
     * mb_format.h snapshot reviewed for this validation, the highest
     * assigned MB_SYS_* value is MB_SYS_RESON7K3 = 41, so 42 is free;
     * re-check this against your exact checkout before building, since
     * new data systems may have been added since this snapshot. */
    *system = MB_SYS_GSFNATIVE;
    *beams_bath_max = MBSYS_GSFNTV_MAX_BEAMS;
    *beams_amp_max = MBSYS_GSFNTV_MAX_BEAMS;
    *pixels_ss_max = 0; /* no sidescan support in this module */

    strncpy(format_name, "GSFNATIV", MB_NAME_LENGTH);
    strncpy(system_name, "gsfnative", MB_NAME_LENGTH);
    strncpy(format_description,
            "Format name: MBF_GSFNATIV\n"
            "Informal Description: Native (non-libgsf) reader for the Generic Sensor\n"
            "  Format (GSF), version 03.11.\n"
            "Attributes: Multibeam bathymetry and amplitude, variable beams,\n"
            "  big-endian binary, records HEADER/SWATHBATHYMETRYPING/\n"
            "  ATTITUDE decoded, all other GSF records skipped by size\n"
            "  (see mbsys_gsfnative.h for full scope).\n",
            MB_DESCRIPTION_LENGTH);

    *numfile = 1;
    *filetype = MB_FILETYPE_SINGLE; /* NOT MB_FILETYPE_GSF -- confirmed in mb_define.h */
    *variable_beams = true;
    *traveltime = false;  /* stores pre-computed depth, not raw travel time */
    *beam_flagging = true;
    *platform_source = MB_DATA_NONE;     /* no sensor-offset record decoded in v1 scope */
    *nav_source = MB_DATA_DATA;          /* position carried directly in the ping header */
    *sensordepth_source = MB_DATA_DATA;  /* ping header depth_corrector/height fields */
    *heading_source = MB_DATA_DATA;
    *attitude_source = MB_DATA_DATA;     /* ping header carries roll/pitch/heave directly;
                                             MB_DATA_ATTITUDE records are also decoded and fed
                                             to mb_attint_add() for interpolation if preferred */
    *svp_source = MB_DATA_NONE;          /* Sound Velocity Profile record not in v1 scope */
    *beamwidth_xtrack = 0.0;             /* GSF's ping/attitude records carry no fixed
                                             system beamwidth; left at 0.0 (unknown) */
    *beamwidth_ltrack = 0.0;

    if (verbose >= 2) {
        fprintf(stderr, "\ndbg2 MBIO function <%s> completed\n", __func__);
        fprintf(stderr, "dbg2 Return status:\n");
        fprintf(stderr, "dbg2  status: %d\n", status);
    }

    return status;
}

/*--------------------------------------------------------------------*/
int mbr_alm_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    /* allocate the persistent record-body read buffer, tracked via the
     * generic mb_io_struct scratch fields (confirmed to exist exactly as
     * named in mb_io.h: void *saveptr1; int save6;), and the data-system
     * storage structure itself */
    mb_io_ptr->saveptr1 = NULL;
    mb_io_ptr->save6 = 0; /* current allocated size of the buffer at saveptr1 */

    /* FIX 3: GSF is always big-endian on disk. mb_swap_check() (mb_swap.c)
     * returns true exactly when the host is NOT big-endian -- i.e. exactly
     * when byte-swapping is required to interpret GSF's bytes correctly on
     * this host. Cache that once here rather than re-deriving it per call. */
    mb_io_ptr->byteswapped = mb_swap_check();

    mb_io_ptr->structure_size = 0;
    mb_io_ptr->data_structure_size = sizeof(struct mbsys_gsfntv_struct);

    status = mbsys_gsfnative_alloc(verbose, mbio_ptr, &mb_io_ptr->store_data, error);

    return status;
}

/*--------------------------------------------------------------------*/
int mbr_dem_gsfnativ(int verbose, void *mbio_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    if (mb_io_ptr->saveptr1 != NULL)
        status = mb_freed(verbose, __FILE__, __LINE__, (void **)&mb_io_ptr->saveptr1, error);
    mb_io_ptr->save6 = 0;

    status = mbsys_gsfnative_deall(verbose, mbio_ptr, &mb_io_ptr->store_data, error);

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_rt_gsfnativ() -- top-level per-call read entry point required by MBIO. */
/*--------------------------------------------------------------------*/
int mbr_rt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    int status = MB_SUCCESS;

    status = mbr_gsfnativ_rd_data(verbose, mbio_ptr, store_ptr, error);

    mb_io_ptr->new_error = *error;
    mb_io_ptr->new_kind = ((struct mbsys_gsfntv_struct *)store_ptr)->kind;

    return status;
}

/*--------------------------------------------------------------------*/
/* mbr_wt_gsfnativ() -- write entry point. NOT YET IMPLEMENTED: this v1
 * module is read-only. Encoding a GSF record back to bytes requires the
 * inverse of every decode step in mbr_gsfnativ_rd_ping()/_rd_attitude()/
 * _rd_header(), plus re-deriving or re-using scale factors and
 * recomputing the 8-byte record header's size field -- all
 * straightforward given the decode logic already written, but
 * deliberately deferred so this first version can be validated against
 * format 121's known-correct output before any write path risks
 * corrupting a file. MB_ERROR_WRITE_FAIL is confirmed to exist in
 * mb_status.h (value 5). */
/*--------------------------------------------------------------------*/
int mbr_wt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    *error = MB_ERROR_WRITE_FAIL;
    fprintf(stderr, "mbr_wt_gsfnativ: write support not implemented in this v1 "
                     "(read-only) native GSF module.\n");
    return MB_FAILURE;
}

/*--------------------------------------------------------------------*/
/* mbr_gsfnativ_rd_data()
 * Reads the 8- or 12-byte record header, reads the full record body
 * into a persistent growable buffer, and dispatches on record type.
 *
 * FIX 4: the previous default case tagged every skipped record with
 * the nonexistent MB_DATA_OTHER. This revision maps each GSF record
 * type to the real, pre-existing MB-System kind already defined for
 * it in mb_status.h. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_data(int verbose, void *mbio_ptr, void *store_ptr, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    struct mbsys_gsfntv_struct *store = (struct mbsys_gsfntv_struct *)store_ptr;
    char header_buf[8];
    char checksum_buf[4];
    unsigned int record_size;
    unsigned int id_word;
    unsigned int body_bytes_remaining;
    int record_type;
    bool checksum_present;
    char *body_buf;
    size_t read_len;
    int status;

    /* --- 1. read the fixed 8-byte size+identifier header --- */
    read_len = (size_t)8;
    status = mb_fileio_get(verbose, mbio_ptr, header_buf, &read_len, error);
    if (status != MB_SUCCESS) {
        store->kind = MB_DATA_NONE;
        return status; /* MB_ERROR_EOF at genuine end of file, or a read failure */
    }

    /* FIX 3: use mb_io_ptr->byteswapped instead of a hardcoded MB_YES. */
    mb_get_binary_int(mb_io_ptr->byteswapped, &header_buf[0], (void *)&record_size);
    mb_get_binary_int(mb_io_ptr->byteswapped, &header_buf[4], (void *)&id_word);

    checksum_present = (id_word & MBSYS_GSFNTV_CHECKSUM_FLAG_MASK) ? true : false;
    record_type = (int)(id_word & MBSYS_GSFNTV_DATATYPE_MASK);

    body_bytes_remaining = record_size;

    /* --- 2. skip the optional 4-byte checksum word --- */
    if (checksum_present) {
        read_len = (size_t)4;
        status = mb_fileio_get(verbose, mbio_ptr, checksum_buf, &read_len, error);
        if (status != MB_SUCCESS) {
            store->kind = MB_DATA_NONE;
            return status;
        }
        body_bytes_remaining -= 4;
        /* Checksum verification (modulo-32 sum) is optional for a read-only
         * module processing files already on disk; not implemented here. */
    }

    /* --- 3. read the remaining record body into a persistent, growable buffer --- */
    if ((unsigned int)mb_io_ptr->save6 < body_bytes_remaining) {
        status = mb_reallocd(verbose, __FILE__, __LINE__, (size_t)body_bytes_remaining,
                              (void **)&mb_io_ptr->saveptr1, error);
        if (status != MB_SUCCESS) {
            mb_io_ptr->save6 = 0;
            store->kind = MB_DATA_NONE;
            return status;
        }
        mb_io_ptr->save6 = (int)body_bytes_remaining;
    }

    body_buf = (char *)mb_io_ptr->saveptr1;

    read_len = (size_t)body_bytes_remaining;
    status = mb_fileio_get(verbose, mbio_ptr, body_buf, &read_len, error);
    if (status != MB_SUCCESS) {
        store->kind = MB_DATA_NONE;
        return status;
    }

    /* --- 4. dispatch on record type --- */
    switch (record_type) {
        case MBSYS_GSFNTV_REC_HEADER:
            status = mbr_gsfnativ_rd_header(verbose, body_buf, store, error);
            store->kind = MB_DATA_HEADER;
            break;

        case MBSYS_GSFNTV_REC_SWATHBATHYMETRYPING:
            status = mbr_gsfnativ_rd_ping(verbose, mbio_ptr, body_buf, (int)body_bytes_remaining, store, error);
            store->kind = MB_DATA_DATA;
            break;

        case MBSYS_GSFNTV_REC_ATTITUDE:
            status = mbr_gsfnativ_rd_attitude(verbose, mbio_ptr, body_buf, store, error);
            store->kind = MB_DATA_ATTITUDE;
            break;

        case MBSYS_GSFNTV_REC_COMMENT:
            /* not decoded in v1 scope, but tagged with the real comment kind */
            store->kind = MB_DATA_COMMENT;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SOUNDVELOCITYPROFILE:
            store->kind = MB_DATA_VELOCITY_PROFILE;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_PROCESSINGPARAMETERS:
            store->kind = MB_DATA_PROCESSING_PARAMETERS;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SENSORPARAMETERS:
            store->kind = MB_DATA_SENSOR_PARAMETERS;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_HISTORY:
            store->kind = MB_DATA_HISTORY;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_NAVIGATIONERROR:
        case MBSYS_GSFNTV_REC_HVNAVIGATIONERROR:
            store->kind = MB_DATA_NAVIGATION_ERROR;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SWATHBATHYSUMMARY:
            store->kind = MB_DATA_SUMMARY;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        case MBSYS_GSFNTV_REC_SINGLEBEAMSOUNDING:
            store->kind = MB_DATA_SINGLE_BEAM_PING;
            status = MB_SUCCESS;
            *error = MB_ERROR_NO_ERROR;
            break;

        default:
            /* Unrecognized record type. The body has already been fully
             * consumed by step 3 above (byte offsets stay correct for the
             * next record); there is no generic "other" kind to fall back
             * to, so this is genuinely unintelligible. */
            store->kind = MB_DATA_NONE;
            status = MB_FAILURE;
            *error = MB_ERROR_UNINTELLIGIBLE;
            break;
    }

    return status;
}

/*--------------------------------------------------------------------*/
/* Header Record -- 12 bytes, plain text, no binary fields to swap. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_header(int verbose, char *buf, struct mbsys_gsfntv_struct *store, int *error) {
    memset(store->header.version, 0, MBSYS_GSFNTV_VERSION_SIZE);
    memcpy(store->header.version, buf, MBSYS_GSFNTV_VERSION_SIZE);

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Swath Bathymetry Ping Record. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_ping(int verbose, void *mbio_ptr, char *buf, int body_len,
                          struct mbsys_gsfntv_struct *store, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    struct mbsys_gsfntv_ping_struct *ping = &(store->ping);
    struct mbsys_gsfntv_pingheader_struct *h = &(ping->header);
    bool swap = mb_io_ptr->byteswapped;
    int index = 0;
    int i;
    short s_tmp;

    /* --- ping header, exactly 56 bytes for GSF v03.01+ --- */
    mb_get_binary_int(swap, &buf[index], (void *)&h->time_sec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->time_nsec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->longitude_raw); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->latitude_raw); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&h->num_beams); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->center_beam); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->ping_flags = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->reserved = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->tide_corrector); index += 2;
    mb_get_binary_int(swap, &buf[index], (void *)&h->depth_corrector); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->heading_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->pitch_raw); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->roll_raw); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&h->heave); index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->course_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_short(swap, &buf[index], (void *)&s_tmp); h->speed_raw = (unsigned short)s_tmp; index += 2;
    mb_get_binary_int(swap, &buf[index], (void *)&h->height); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->separation); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&h->gps_tide_corrector); index += 4;
    mb_get_binary_short(swap, &buf[index], (void *)&h->spare); index += 2;
    /* index now equals 56, verified by direct field-width summation */

    if (h->num_beams < 0 || h->num_beams > MBSYS_GSFNTV_MAX_BEAMS) {
        *error = MB_ERROR_UNINTELLIGIBLE;
        return MB_FAILURE;
    }

    /* --- decode the header's directly-usable scalar values --- */
    store->time_d = (double)h->time_sec + (double)h->time_nsec / 1.0e9;
    mb_get_date(verbose, store->time_d, store->time_i);

    ping->navlon = (double)h->longitude_raw / 10000000.0;
    ping->navlat = (double)h->latitude_raw / 10000000.0;
    ping->heading = (double)h->heading_raw / 100.0;
    ping->speed = (double)h->speed_raw / 100.0; /* knots */

    /* --- walk the remaining subrecords until the ping body is exhausted --- */
    while (index < body_len) {
        unsigned int subrecord_word;
        int subrecord_id, subrecord_size;

        if (index + 4 > body_len)
            break; /* malformed trailing bytes; stop rather than read past the buffer */

        mb_get_binary_int(swap, &buf[index], (void *)&subrecord_word); index += 4;

        /* TODO-VERIFY-VS-SPEC: this bit split (12-bit id, 20-bit size) is a
         * GSF-Spec question (Figure 4-6), not resolvable from MB-System
         * source; confirm against the actual GSF Spec PDF before trusting
         * this in production. */
        subrecord_id = (int)((subrecord_word >> 20) & 0x00000FFFu);
        subrecord_size = (int)(subrecord_word & 0x000FFFFFu);

        if (subrecord_size < 0 || index + subrecord_size > body_len)
            break; /* declared size runs past the record; stop defensively */

        switch (subrecord_id) {
            case MBSYS_GSFNTV_SUB_SCALEFACTORS:
                mbr_gsfnativ_rd_scalefactors(verbose, mbio_ptr, &buf[index], &ping->scalefactors, error);
                break;

            case MBSYS_GSFNTV_SUB_DEPTHARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_DEPTHARRAY, ping->depth, error);
                break;

            case MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY, ping->across_track, error);
                break;

            case MBSYS_GSFNTV_SUB_ALONGTRACKARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_ALONGTRACKARRAY, ping->along_track, error);
                break;

            case MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY:
                mbr_gsfnativ_rd_beamarray(verbose, mbio_ptr, &buf[index], h->num_beams,
                                          &ping->scalefactors, MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY, ping->amplitude, error);
                break;

            case MBSYS_GSFNTV_SUB_BEAMFLAGSARRAY:
                /* always 1 byte per beam, never scaled */
                for (i = 0; i < h->num_beams; i++)
                    ping->beam_flags[i] = (unsigned char)buf[index + i];
                break;

            default:
                /* any subrecord not in this module's v1 scope (travel time, beam
                 * angle, quality factor, receive heave, nominal depth, quality
                 * flags, signal-to-noise, beam-angle-forward, vertical/horizontal
                 * error, sensor-specific, imagery, etc.) -- already accounted for
                 * by advancing index below; simply not decoded */
                break;
        }

        index += subrecord_size;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Scale-factor subrecord. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_scalefactors(int verbose, void *mbio_ptr, char *buf,
                                  struct mbsys_gsfntv_scalefactors_struct *sf, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    int index = 0;
    int i;
    unsigned int id_and_flags;

    mb_get_binary_int(swap, &buf[index], (void *)&sf->num_factors); index += 4;

    if (sf->num_factors < 0)
        sf->num_factors = 0;
    if (sf->num_factors > MBSYS_GSFNTV_MAX_SCALEFACTORS)
        sf->num_factors = MBSYS_GSFNTV_MAX_SCALEFACTORS;

    for (i = 0; i < sf->num_factors; i++) {
        mb_get_binary_int(swap, &buf[index], (void *)&id_and_flags); index += 4;

        /* TODO-VERIFY-VS-SPEC: this split is a GSF-Spec question (Figure 4-7),
         * not resolvable from MB-System source; it is modeled on GSFlib's
         * documented "cflag" byte packing (field size high nibble,
         * compression flag low nibble) alongside the subrecord id. Confirm
         * against the actual GSF Spec PDF before trusting in production. */
        sf->factor[i].subrecord_id = (int)((id_and_flags >> 8) & 0x00FFFFFFu);
        sf->factor[i].compression_flag = (int)(id_and_flags & 0x0000000Fu);

        {
            int size_selector = (int)((id_and_flags >> 4) & 0x0000000Fu);
            if (size_selector == 1) sf->factor[i].field_size = 1;
            else if (size_selector == 3) sf->factor[i].field_size = 4;
            else sf->factor[i].field_size = 2; /* default / selector 2 */
        }

        mb_get_binary_float(swap, &buf[index], (void *)&sf->factor[i].multiplier); index += 4;
        mb_get_binary_float(swap, &buf[index], (void *)&sf->factor[i].offset); index += 4;
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Generic scaled beam-array decoder. */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_beamarray(int verbose, void *mbio_ptr, char *buf, int num_beams,
                               struct mbsys_gsfntv_scalefactors_struct *sf, int target_subrecord_id,
                               double *out_array, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    float multiplier = 1.0f, offset = 0.0f;
    int field_size = 2; /* default per-element width if no matching scale factor is found */
    int i, found = 0;
    int raw;
    short s_raw;

    for (i = 0; i < sf->num_factors; i++) {
        if (sf->factor[i].subrecord_id == target_subrecord_id) {
            multiplier = sf->factor[i].multiplier;
            offset = sf->factor[i].offset;
            field_size = sf->factor[i].field_size;
            found = 1;
            break;
        }
    }

    if (!found || multiplier == 0.0f)
        multiplier = (multiplier == 0.0f) ? 1.0f : multiplier; /* guard divide-by-zero */

    for (i = 0; i < num_beams; i++) {
        if (field_size == 4) {
            mb_get_binary_int(swap, &buf[4 * i], (void *)&raw);
        } else if (field_size == 1) {
            raw = (unsigned char)buf[i];
        } else {
            mb_get_binary_short(swap, &buf[2 * i], (void *)&s_raw);
            raw = s_raw;
        }

        out_array[i] = mbsys_gsfnative_decode_scaled(raw, multiplier, offset);
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}

/*--------------------------------------------------------------------*/
/* Attitude Record. Decodes the record AND feeds every sample into
 * MBIO's generic asynchronous-attitude interpolation buffer via
 * mb_attint_add(), so that mb_attint_interp() can later be called (by
 * this module's own extract_nav() or by an application) to obtain
 * roll/pitch/heave interpolated onto any survey-record timestamp.
 * mb_attint_add()'s parameter order is CONFIRMED against mb_define.h:
 * int mb_attint_add(int verbose, void *mbio_ptr, double time_d,
 *                    double heave, double roll, double pitch, int *error); */
/*--------------------------------------------------------------------*/
int mbr_gsfnativ_rd_attitude(int verbose, void *mbio_ptr, char *buf,
                              struct mbsys_gsfntv_struct *store, int *error) {
    struct mb_io_struct *mb_io_ptr = (struct mb_io_struct *)mbio_ptr;
    bool swap = mb_io_ptr->byteswapped;
    struct mbsys_gsfntv_attitude_struct *att = &(store->attitude);
    int index = 0;
    int i;
    short offset_raw; /* TODO-VERIFY-VS-SPEC: unit assumed milliseconds */
    short s_val;
    unsigned short us_val;
    int add_status, add_error;

    mb_get_binary_int(swap, &buf[index], (void *)&att->base_time_sec); index += 4;
    mb_get_binary_int(swap, &buf[index], (void *)&att->base_time_nsec); index += 4;
    att->base_time_d = (double)att->base_time_sec + (double)att->base_time_nsec / 1.0e9;

    mb_get_binary_short(swap, &buf[index], (void *)&s_val);
    att->num_measurements = (int)s_val;
    index += 2;

    if (att->num_measurements < 0)
        att->num_measurements = 0;
    if (att->num_measurements > MBSYS_GSFNTV_MAX_ATTITUDE)
        att->num_measurements = MBSYS_GSFNTV_MAX_ATTITUDE;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&offset_raw);
        att->sample_time_d[i] = att->base_time_d + (double)offset_raw / 1000.0; /* assumed ms */
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->pitch[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->roll[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        att->heave[i] = (double)s_val / 100.0;
    }
    index += 2 * att->num_measurements;

    for (i = 0; i < att->num_measurements; i++) {
        mb_get_binary_short(swap, &buf[index + 2 * i], (void *)&s_val);
        us_val = (unsigned short)s_val;
        att->heading[i] = (double)us_val / 100.0;
    }
    index += 2 * att->num_measurements;

    store->time_d = att->base_time_d;
    mb_get_date(verbose, store->time_d, store->time_i);

    /* --- on-the-fly interpolation feed, confirmed parameter order --- */
    for (i = 0; i < att->num_measurements; i++) {
        add_error = MB_ERROR_NO_ERROR;
        add_status = mb_attint_add(verbose, mbio_ptr, att->sample_time_d[i],
                                    att->heave[i], att->roll[i], att->pitch[i], &add_error);
        if (add_status != MB_SUCCESS && verbose >= 1)
            fprintf(stderr, "mbr_gsfnativ_rd_attitude: mb_attint_add() failed, error %d\n", add_error);
    }

    *error = MB_ERROR_NO_ERROR;
    return MB_SUCCESS;
}
