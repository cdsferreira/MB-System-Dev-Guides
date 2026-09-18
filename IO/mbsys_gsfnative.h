/*--------------------------------------------------------------------
 * mbsys_gsfnative.h  --  VALIDATED v4
 *
 * MB-System data-system header for a NATIVE (non-libgsf) reader of
 * the Generic Sensor Format (GSF), version 03.11.
 *
 * Registered as:
 *   Data system name : gsfnative      (MB_SYS_GSFNATIVE, suggested = 42)
 *   Format name      : GSFNATIV       (8 characters, suggested format id = 300)
 *
 * This module is independent of MB-System's existing GSF support
 * (format 121, MBF_GSFGENMB / mbsys_gsf.*), which wraps the external
 * Leidos libgsf C library. This module parses the documented GSF
 * byte stream directly.
 *
 * VALIDATION NOTE (this v4 revision): this header, its companion
 * mbsys_gsfnative.c and mbr_gsfnativ.c, were cross-checked line by
 * line against the following real MB-System 5.8.3beta16 source
 * files supplied by the user: mb_io.h, mb_define.h, mb_status.h,
 * mb_format.h, mb_format.c, mb_swap.c, mb_mem.c, mb_fileio.c,
 * mb_time.c, mbsys_templatesystem.h/.c, mbr_tempform.c,
 * mbsys_gsf.h/.c, mbr_gsfgenmb.c, and src/mbio/Makefile.am. Every
 * MBIO-side struct field, function-pointer slot, and helper-function
 * signature referenced below was verified against those files (not
 * assumed). Only bit-level packing that is defined by the GSF Spec
 * itself (not by MB-System) remains flagged TODO-VERIFY, since no
 * MB-System source file can confirm GSF's own on-disk bit layout.
 *
 * SCOPE (v1): read-only. Handles GSF record types HEADER (1),
 * SWATHBATHYMETRYPING (2), and ATTITUDE (12) with full decoding.
 * Every other GSF record type is read as an opaque, length-known
 * block and skipped, but is now correctly tagged with the exact
 * MB-System record kind that already exists for it (see the
 * dispatch table in mbr_gsfnativ.c) instead of the previously
 * invented and nonexistent MB_DATA_OTHER.
 *
 * Within the ping record, only these array subrecords are decoded:
 * DEPTHARRAY (1), ACROSSTRACKARRAY (2), ALONGTRACKARRAY (3),
 * MEANCALAMPLITUDEARRAY (6), BEAMFLAGSARRAY (16), and the
 * SCALEFACTORS (100) subrecord required to decode the numeric
 * arrays. All other subrecords are skipped by declared size.
 *
 * OPEN ITEMS still requiring confirmation against the GSF Spec PDF
 * itself (these are GSF-format questions, not MB-System-API
 * questions, so no amount of MB-System source review resolves them):
 *   1. Exact bit packing of the scale-factor entry's id/flags word
 *      (Spec Figure 4-7).
 *   2. Exact bit packing of the ping-body subrecord header word
 *      (Spec Figure 4-6).
 *   3. Physical unit of the Attitude record's ATTITUDETIME offset
 *      field (assumed milliseconds here).
 * Each is marked at its point of use with a "TODO-VERIFY-VS-SPEC"
 * comment. Items resolved in this revision by MB-System source
 * review are marked "RESOLVED v4" at their point of use.
 *------------------------------------------------------------------*/

#ifndef MBSYS_GSFNATIVE_H_
#define MBSYS_GSFNATIVE_H_

/*--------------------------------------------------------------------
 * Record and subrecord identifiers (GSF Spec Appendix A.1 / A.2)
 *------------------------------------------------------------------*/

/* GSF top-level record identifiers -- registry 0 */
#define MBSYS_GSFNTV_REC_HEADER                1
#define MBSYS_GSFNTV_REC_SWATHBATHYMETRYPING    2
#define MBSYS_GSFNTV_REC_SOUNDVELOCITYPROFILE    3
#define MBSYS_GSFNTV_REC_PROCESSINGPARAMETERS    4
#define MBSYS_GSFNTV_REC_SENSORPARAMETERS      5
#define MBSYS_GSFNTV_REC_COMMENT               6
#define MBSYS_GSFNTV_REC_HISTORY               7
#define MBSYS_GSFNTV_REC_NAVIGATIONERROR       8  /* obsolete since GSF v1.07 */
#define MBSYS_GSFNTV_REC_SWATHBATHYSUMMARY      9
#define MBSYS_GSFNTV_REC_SINGLEBEAMSOUNDING    10  /* discouraged since GSF v2.03 */
#define MBSYS_GSFNTV_REC_HVNAVIGATIONERROR     11
#define MBSYS_GSFNTV_REC_ATTITUDE              12

/* GSF swath bathymetry ping subrecord identifiers */
#define MBSYS_GSFNTV_SUB_DEPTHARRAY             1
#define MBSYS_GSFNTV_SUB_ACROSSTRACKARRAY       2
#define MBSYS_GSFNTV_SUB_ALONGTRACKARRAY        3
#define MBSYS_GSFNTV_SUB_TRAVELTIMEARRAY        4
#define MBSYS_GSFNTV_SUB_BEAMANGLEARRAY         5
#define MBSYS_GSFNTV_SUB_MEANCALAMPLITUDEARRAY  6
#define MBSYS_GSFNTV_SUB_MEANRELAMPLITUDEARRAY  7
#define MBSYS_GSFNTV_SUB_ECHOWIDTHARRAY         8
#define MBSYS_GSFNTV_SUB_QUALITYFACTORARRAY     9
#define MBSYS_GSFNTV_SUB_RECEIVEHEAVEARRAY     10
#define MBSYS_GSFNTV_SUB_NOMINALDEPTHARRAY     14
#define MBSYS_GSFNTV_SUB_QUALITYFLAGSARRAY     15  /* obsolete */
#define MBSYS_GSFNTV_SUB_BEAMFLAGSARRAY        16
#define MBSYS_GSFNTV_SUB_SIGNALTONOISEARRAY    17
#define MBSYS_GSFNTV_SUB_BEAMANGLEFORWARDARRAY 18
#define MBSYS_GSFNTV_SUB_VERTICALERRORARRAY    19
#define MBSYS_GSFNTV_SUB_HORIZONTALERRORARRAY  20
#define MBSYS_GSFNTV_SUB_SCALEFACTORS          100

/* Top-level record identifier word bit masks (Spec Sec 4.3.1.1/4.3.1.2).
 * RESOLVED v4: removed the previous pre-shifted MBSYS_GSFNTV_REGISTRY_MASK
 * constant, which was numerically wrong (0x000FFC00u) and was, in any case,
 * dead code never referenced by the actual header-parsing logic. Registry
 * extraction (unused by this v1 module but provided for completeness) is
 * now expressed the same shift-then-mask way as the already-correct
 * data-type extraction below. */
#define MBSYS_GSFNTV_CHECKSUM_FLAG_MASK  0x80000000u  /* bit 31 */
#define MBSYS_GSFNTV_DATATYPE_MASK       0x00000FFFu  /* bits 11-0 */
#define MBSYS_GSFNTV_REGISTRY_SHIFT      12
#define MBSYS_GSFNTV_REGISTRY_MASK       0x000003FFu  /* apply AFTER >> 12 */

/* Ping flag bit (Spec Appendix C.1) */
#define MBSYS_GSFNTV_PINGFLAG_IGNORE 0x0001

/* Beam flag bits (Spec Appendix C.2) */
#define MBSYS_GSFNTV_BEAMFLAG_IGNORE   0x01
#define MBSYS_GSFNTV_BEAMFLAG_SELECTED 0x02

/*--------------------------------------------------------------------
 * Array size limits
 *------------------------------------------------------------------*/
#define MBSYS_GSFNTV_MAX_BEAMS        1024
#define MBSYS_GSFNTV_MAX_SCALEFACTORS   32
#define MBSYS_GSFNTV_MAX_ATTITUDE     1024
#define MBSYS_GSFNTV_VERSION_SIZE       12

/*--------------------------------------------------------------------
 * Header Record -- 12 bytes, one text field
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_header_struct {
    char version[MBSYS_GSFNTV_VERSION_SIZE]; /* e.g. "GSF-v03.11", not guaranteed null-terminated on disk */
};

/*--------------------------------------------------------------------
 * Swath Bathymetry Ping Record -- Ping Header (56 bytes, GSF v03.01+)
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_pingheader_struct {
    int            time_sec;           /* POSIX seconds */
    int            time_nsec;          /* nanoseconds within the second */
    int            longitude_raw;      /* 1e-7 degree, signed, +East */
    int            latitude_raw;       /* 1e-7 degree, signed, +North */
    short          num_beams;          /* N */
    short          center_beam;        /* column index nearest keel */
    unsigned short ping_flags;
    unsigned short reserved;
    short          tide_corrector;     /* centimeters, already applied to depths */
    int            depth_corrector;    /* centimeters, already applied to depths */
    unsigned short heading_raw;        /* hundredths of a degree, 0-36000 */
    short          pitch_raw;          /* hundredths of a degree, signed */
    short          roll_raw;           /* hundredths of a degree, signed */
    short          heave;              /* centimeters, signed */
    unsigned short course_raw;         /* hundredths of a degree */
    unsigned short speed_raw;          /* hundredths of a knot */
    int            height;             /* 0.001 m, GSF v03.01+ only */
    int            separation;         /* 0.001 m, GSF v03.01+ only */
    int            gps_tide_corrector; /* millimeters, GSF v03.01+ only */
    short          spare;
};

/*--------------------------------------------------------------------
 * Scale-factor subrecord
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_scalefactor_entry_struct {
    int   subrecord_id;    /* which array subrecord this scale factor applies to */
    int   field_size;      /* decoded element width in bytes: 1, 2, or 4 */
    int   compression_flag;
    float multiplier;
    float offset;
};

struct mbsys_gsfntv_scalefactors_struct {
    int num_factors; /* M */
    struct mbsys_gsfntv_scalefactor_entry_struct factor[MBSYS_GSFNTV_MAX_SCALEFACTORS];
};

/*--------------------------------------------------------------------
 * Swath Bathymetry Ping Record -- decoded ping
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_ping_struct {
    struct mbsys_gsfntv_pingheader_struct   header;
    struct mbsys_gsfntv_scalefactors_struct scalefactors; /* persists until a new instance appears */

    double navlon;   /* decimal degrees */
    double navlat;   /* decimal degrees */
    double heading;  /* decimal degrees */
    double speed;    /* knots */

    double        depth[MBSYS_GSFNTV_MAX_BEAMS];        /* meters */
    double        across_track[MBSYS_GSFNTV_MAX_BEAMS]; /* meters */
    double        along_track[MBSYS_GSFNTV_MAX_BEAMS];  /* meters */
    double        amplitude[MBSYS_GSFNTV_MAX_BEAMS];    /* dB */
    unsigned char beam_flags[MBSYS_GSFNTV_MAX_BEAMS];   /* raw 8-bit GSF beam flag */
};

/*--------------------------------------------------------------------
 * Attitude record
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_attitude_struct {
    int    base_time_sec;
    int    base_time_nsec;
    double base_time_d;
    int    num_measurements; /* N, per record: <= 60 sec worth */
    double sample_time_d[MBSYS_GSFNTV_MAX_ATTITUDE];
    double pitch[MBSYS_GSFNTV_MAX_ATTITUDE];
    double roll[MBSYS_GSFNTV_MAX_ATTITUDE];
    double heave[MBSYS_GSFNTV_MAX_ATTITUDE];
    double heading[MBSYS_GSFNTV_MAX_ATTITUDE];
};

/*--------------------------------------------------------------------
 * Primary data-system structure
 *------------------------------------------------------------------*/
struct mbsys_gsfntv_struct {
    int    kind; /* MB_DATA_DATA, MB_DATA_ATTITUDE, MB_DATA_HEADER, MB_DATA_NONE,
                    or one of the pre-existing GSF-specific kinds already defined
                    in mb_status.h for skipped record types -- see mbr_gsfnativ.c */
    double time_d;
    int    time_i[7];

    struct mbsys_gsfntv_header_struct    header;
    struct mbsys_gsfntv_ping_struct      ping;
    struct mbsys_gsfntv_attitude_struct  attitude;
};

/*--------------------------------------------------------------------
 * Function prototypes -- mbsys_gsfnative.c
 * VALIDATED against struct mb_io_struct's function-pointer slots
 * in mb_io.h. Fixes applied in this revision:
 *   - mbsys_gsfnative_pingnumber(): pingnumber is unsigned int*,
 *     matching mb_io_ptr->mb_io_pingnumber exactly (was int* before).
 *------------------------------------------------------------------*/

int mbsys_gsfnative_alloc(int verbose, void *mbio_ptr, void **store_ptr, int *error);

int mbsys_gsfnative_deall(int verbose, void *mbio_ptr, void **store_ptr, int *error);

int mbsys_gsfnative_dimensions(int verbose, void *mbio_ptr, void *store_ptr,
                                int *kind, int *nbath, int *namp, int *nss, int *error);

int mbsys_gsfnative_pingnumber(int verbose, void *mbio_ptr, unsigned int *pingnumber, int *error);

int mbsys_gsfnative_sonartype(int verbose, void *mbio_ptr, void *store_ptr,
                               int *sonartype, int *error);

int mbsys_gsfnative_extract(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int time_i[7], double *time_d,
                             double *navlon, double *navlat, double *speed, double *heading,
                             int *nbath, int *namp, int *nss,
                             char *beamflag, double *bath, double *amp,
                             double *bathacrosstrack, double *bathalongtrack,
                             double *ss, double *ssacrosstrack, double *ssalongtrack,
                             char *comment, int *error);

int mbsys_gsfnative_insert(int verbose, void *mbio_ptr, void *store_ptr,
                            int kind, int time_i[7], double time_d,
                            double navlon, double navlat, double speed, double heading,
                            int nbath, int namp, int nss,
                            char *beamflag, double *bath, double *amp,
                            double *bathacrosstrack, double *bathalongtrack,
                            double *ss, double *ssacrosstrack, double *ssalongtrack,
                            char *comment, int *error);

int mbsys_gsfnative_extract_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                 int *kind, int time_i[7], double *time_d,
                                 double *navlon, double *navlat, double *speed, double *heading,
                                 double *draft, double *roll, double *pitch, double *heave, int *error);

int mbsys_gsfnative_insert_nav(int verbose, void *mbio_ptr, void *store_ptr,
                                int time_i[7], double time_d,
                                double navlon, double navlat, double speed, double heading,
                                double draft, double roll, double pitch, double heave, int *error);

int mbsys_gsfnative_extract_altitude(int verbose, void *mbio_ptr, void *store_ptr,
                                      int *kind, double *transducer_depth, double *altitude, int *error);

int mbsys_gsfnative_ttimes(int verbose, void *mbio_ptr, void *store_ptr,
                            int *kind, int *nbeams,
                            double *ttime, double *angle_xtrack, double *angle_ltrack,
                            double *heave, double *alongtrack_offset,
                            double *draft, double *ssv, int *error);

int mbsys_gsfnative_detects(int verbose, void *mbio_ptr, void *store_ptr,
                             int *kind, int *nbeams, int *detects, int *error);

int mbsys_gsfnative_gains(int verbose, void *mbio_ptr, void *store_ptr,
                           int *kind, double *transmit_gain, double *pulse_length,
                           double *receive_gain, int *error);

int mbsys_gsfnative_copy(int verbose, void *mbio_ptr,
                          void *store_ptr, void *copy_ptr, int *error);

/*--------------------------------------------------------------------
 * Function prototypes -- mbr_gsfnativ.c
 * VALIDATED against mbr_tempform.c / mb_format.h. Fix applied:
 *   - mbr_info_gsfnativ(): added platform_source, sensordepth_source;
 *     renamed vru_source -> attitude_source; variable_beams/
 *     traveltime/beam_flagging changed from int* to bool*, matching
 *     the real signature used by EVERY mbr_info_XXX() in mb_format.h.
 *------------------------------------------------------------------*/

int mbr_register_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_info_gsfnativ(int verbose,
                       int *system, int *beams_bath_max, int *beams_amp_max, int *pixels_ss_max,
                       char *format_name, char *system_name, char *format_description,
                       int *numfile, int *filetype,
                       bool *variable_beams, bool *traveltime, bool *beam_flagging,
                       int *platform_source, int *nav_source, int *sensordepth_source,
                       int *heading_source, int *attitude_source, int *svp_source,
                       double *beamwidth_xtrack, double *beamwidth_ltrack, int *error);

int mbr_alm_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_dem_gsfnativ(int verbose, void *mbio_ptr, int *error);

int mbr_rt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error);

int mbr_wt_gsfnativ(int verbose, void *mbio_ptr, void *store_ptr, int *error);

/* internal helper functions, used only within mbr_gsfnativ.c */
int mbr_gsfnativ_rd_data(int verbose, void *mbio_ptr, void *store_ptr, int *error);
int mbr_gsfnativ_rd_header(int verbose, char *buf, struct mbsys_gsfntv_struct *store, int *error);
int mbr_gsfnativ_rd_ping(int verbose, void *mbio_ptr, char *buf, int body_len,
                          struct mbsys_gsfntv_struct *store, int *error);
int mbr_gsfnativ_rd_scalefactors(int verbose, void *mbio_ptr, char *buf,
                                  struct mbsys_gsfntv_scalefactors_struct *sf, int *error);
int mbr_gsfnativ_rd_beamarray(int verbose, void *mbio_ptr, char *buf, int num_beams,
                               struct mbsys_gsfntv_scalefactors_struct *sf, int target_subrecord_id,
                               double *out_array, int *error);
int mbr_gsfnativ_rd_attitude(int verbose, void *mbio_ptr, char *buf,
                              struct mbsys_gsfntv_struct *store, int *error);

double mbsys_gsfnative_decode_scaled(int raw_value, float multiplier, float offset);
int    mbsys_gsfnative_encode_scaled(double real_value, float multiplier, float offset);

#endif /* MBSYS_GSFNATIVE_H_ */
