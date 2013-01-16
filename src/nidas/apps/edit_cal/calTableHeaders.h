#ifndef _calTableHeaders_h_
#define _calTableHeaders_h_

#define DB_DRIVER  "QPSQL"
#define DB_HOST    "shiraz.eol.ucar.edu"
#define DB_USER    "ads"
#define DB_NAME    "calibrations"
#define MAX_ORDER  3

//#define SANDBOX

#ifdef SANDBOX
#define DB_TABLE   "sandbox"
#else
#define DB_TABLE   "calibrations"
#endif

enum calTableHeaders {
    clm_rid,
    clm_pid,
    clm_site,
    clm_pulled,
    clm_removed,
    clm_exported,
    clm_cal_date,
    clm_project_name,
    clm_username,
    clm_sensor_type,
    clm_serial_number,
    clm_var_name,
    clm_dsm_name,
    clm_cal_type,
    clm_channel,
    clm_gainbplr,
    clm_ads_file_name,
    clm_set_times,
    clm_set_points,
    clm_averages,
    clm_stddevs,
    clm_cal,
    clm_temperature,
    clm_comment,
    clm_COUNT
};

#endif
