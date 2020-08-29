#pragma once
#include <esp_system.h>

enum  cv_api_code{
    CV_OK,
    CV_ERROR_NO_COMMS,
    CV_ERROR_INVALID_ENDPOINT,
    CV_ERROR_WRITE,
    CV_ERROR_READ,
    CV_ERROR_VALUE,
    CV_ERROR_NVS_READ,
    CV_ERROR_NVS_WRITE
};


struct cv_api_write {
    bool success; // Whether parameter was written
    enum cv_api_code api_code;
};

struct cv_api_read {
    bool success; // Whether parameter was read
    enum cv_api_code api_code; 
    char* val; // The value of the parameter, or error message if not succesful
};

extern int form_command(char* payload, char* output_command, int bufsz);

//https://stackoverflow.com/q/10162152/14180509

// CV Getters
extern void get_channel(struct cv_api_read* ret);
extern void get_custom_report(char* report, struct cv_api_read* ret);



// CV Setters
extern void set_address(char* address, struct cv_api_write* ret );
extern void set_antenna(char* antenna, struct cv_api_write* ret );
extern void set_channel(char* channel, struct cv_api_write* ret );
extern void set_band(char* band, struct cv_api_write* ret );
extern void set_id(char* id_str, struct cv_api_write* ret );
extern void set_usermsg(char* usermsg_str, struct cv_api_write* ret );
extern void set_mode(char* mode_str, struct cv_api_write* ret );
extern void set_osdvis(char* osdvis_str, struct cv_api_write* ret );
extern void set_osdpos(char* osdpos, struct cv_api_write* ret );
extern void reset_lock(struct cv_api_write* ret );
extern void set_videoformat(char* videoformat, struct cv_api_write* ret );
extern void set_custom_w(char* cmd, struct cv_api_write* ret );
