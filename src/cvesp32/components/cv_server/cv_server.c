#ifndef CV_SERVER_C
#define CV_SERVER_C

#define WRITE_THEN_READ 1


#include <esp_event.h>
#include <esp_log.h>
#include <cJSON.h>
#include <cJSON_Utils.h>


#include "lwip/api.h"

#include "cv_utils.h"
#include "cv_mqtt.h"
#include "cv_uart.h"
#include "cv_ota.h"
#include "cv_ledc.h"
#include "cv_api.h"
#include "cv_utils.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b)) //where does this come from? see http_server_simple example
#endif //MIN

#define POST_BUFFER_SZ 200

const char* TAG_SERVER = "CV_SERVER";
static bool _server_started = false;

extern const uint8_t title_html_start[] asm("_binary_title_html_start");
extern const uint8_t title_html_end[]   asm("_binary_title_html_end");

extern const uint8_t subtitle_html_start[] asm("_binary_subtitle_html_start");
extern const uint8_t subtitle_html_end[]   asm("_binary_subtitle_html_end");

extern const uint8_t wifiSettings_html_start[] asm("_binary_wifiSettings_html_start");
extern const uint8_t wifiSettings_html_end[]   asm("_binary_wifiSettings_html_end");

extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
extern const uint8_t settings_html_end[]   asm("_binary_settings_html_end");

extern const uint8_t settings_html_start[] asm("_binary_settings_html_start");
extern const uint8_t settings_html_end[]   asm("_binary_settings_html_end");

extern const uint8_t test_html_start[] asm("_binary_test_html_start");
extern const uint8_t test_html_end[]   asm("_binary_test_html_end");

extern const uint8_t html_beg_start[] asm("_binary_httpDocBegin_html_start");
extern const uint8_t html_beg_end[] asm("_binary_httpDocBegin_html_end");

extern const uint8_t html_conc_start[] asm("_binary_httpDocConclude_html_start");
extern const uint8_t html_conc_end[] asm("_binary_httpDocConclude_html_end");

extern const uint8_t cv_js_start[] asm("_binary_cv_js_js_start");
extern const uint8_t cv_js_end[] asm("_binary_cv_js_js_end");

#ifdef CONFIG_CV_INITIAL_PROGRAM
extern const uint8_t menu_bar_start[] asm("_binary_menuBarTest_html_start");
extern const uint8_t menu_bar_end[] asm("_binary_menuBarTest_html_end");
#else
extern const uint8_t menu_bar_start[] asm("_binary_menuBar_html_start");
extern const uint8_t menu_bar_end[] asm("_binary_menuBar_html_end");
#endif



// URI Defines
#define CV_CONFIG_WIFI_URI "/wifi"
#define CV_CONFIG_WIFI_INSTANT_URI "/wifi_instant"

// API ENDPOINTS
#define CV_API_ADDRESS "address"
#define CV_API_SSID "ssid"
#define CV_API_PASSWORD "password"
#define CV_API_DEVICE_NAME "device_name"
#define CV_API_BROKER_IP "broker_ip"
#define CV_API_SEAT "seat"
#define CV_API_ANTENNA "antenna"
#define CV_API_CHANNEL "channel"
#define CV_API_BAND "band"
#define CV_API_ID "id"
#define CV_API_USER_MESSAGE "user_msg"
#define CV_API_MODE "mode"
#define CV_API_OSD_VISIBILITY "osd_visibility"
#define CV_API_OSD_POSITION "osd_position"
#define CV_API_LOCK "lock"
#define CV_API_VIDEO_FORMAT "video_format"
#define CV_API_CV_VERSION "cv_version"
#define CV_API_CVCM_VERSION "cvcm_version"
#define CV_API_CVCM_VERSION_ALL "cvcm_version_all"
#define CV_API_LED "led"
#define CV_API_SEND_CMD "send_cmd"
#define CV_API_REQ_REPORT "req_report"
#define CV_API_MAC_ADDR "mac_addr"

#define CV_READ_Q "?"

//TODO make these a struct
bool ssid_ready = false;
bool password_ready = false;
bool device_name_ready = false;
bool broker_ip_ready = false;

//Return 0 if content type is json
static int is_content_json(httpd_req_t *req){
    //Check content type
    const char CSZ = 50;
    char content_type[CSZ];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, CSZ);
    return strncmp(HTTPD_TYPE_JSON, content_type, strlen(content_type));
}

static int is_content_html_plain(httpd_req_t *req){
    //Check content type
    const char CSZ = 50;
    char content_type[CSZ];
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, CSZ);
    return strncmp(HTTPD_TYPE_TEXT, content_type, strlen(content_type));
}

void add_response_to_json_car(cJSON* ret, char* k, struct cv_api_read* car){
    if (car->success) {
        cJSON_AddStringToObject(ret, k, car->val);
    }
    else {
        if (car->api_code == CV_ERROR_NO_COMMS) {cJSON_AddStringToObject(ret, k, "error-no_comms");}
        else if (car->api_code == CV_ERROR_WRITE) {cJSON_AddStringToObject(ret, k, "error-write_on_read_only_param");} 
        else if (car->api_code == CV_ERROR_READ) {cJSON_AddStringToObject(ret, k, "error-read_on_write_only_param");} 
        else if (car->api_code == CV_ERROR_VALUE) {cJSON_AddStringToObject(ret, k, "error-value");} 
        else if (car->api_code == CV_ERROR_INVALID_ENDPOINT) {cJSON_AddStringToObject(ret, k, "error-invalid_endpoint");}
        else if (car->api_code == CV_ERROR_NVS_WRITE) {cJSON_AddStringToObject(ret, k, "error-nvs_write");}
        else if (car->api_code == CV_ERROR_NVS_READ) {cJSON_AddStringToObject(ret, k, "error-nvs_read");}
        else {CV_LOGE("artjc", "Unknown car->api_code");}
    }
}

void add_response_to_json_caw(cJSON* ret, char* k, struct cv_api_write* caw, char* v) {
    if (caw->success) {
        cJSON_AddStringToObject(ret, k, v);
    }
    else {
        if (caw->api_code == CV_ERROR_NO_COMMS) {cJSON_AddStringToObject(ret, k, "error-no_comms");}
        else if (caw->api_code == CV_ERROR_WRITE) {cJSON_AddStringToObject(ret, k, "error-write");} 
        else if (caw->api_code == CV_ERROR_READ) {cJSON_AddStringToObject(ret, k, "error-read");} 
        else if (caw->api_code == CV_ERROR_VALUE) {cJSON_AddStringToObject(ret, k, "error-value");} 
        else if (caw->api_code == CV_ERROR_INVALID_ENDPOINT) {cJSON_AddStringToObject(ret, k, "error-invalid_endpoint");}
        else if (caw->api_code == CV_ERROR_NVS_WRITE) {cJSON_AddStringToObject(ret, k, "error-nvs_write");}
        else if (caw->api_code == CV_ERROR_NVS_READ) {cJSON_AddStringToObject(ret, k, "error-nvs_read");}
        else {CV_LOGE("artjc", "Unknown caw->api_code");}
    }
}

// Parse read request, execute it, and fill the data in car
void kv_api_parse_car(struct cv_api_read* car, char* k, char* v) {
    char* TAG = "cv_server->kv_api_parse_car";
    CV_LOGI(TAG, "Getting value for '%s'",k);
        
    if (strncmp(k, CV_API_CHANNEL, strlen(k)) == 0){
        get_channel(car);
    }else if (strncmp(k, CV_API_BAND, strlen(k)) == 0){
        get_band(car);
    }else if (strncmp(k, CV_API_ID, strlen(k)) == 0){
        get_id(car);
    }else if (strncmp(k, CV_API_REQ_REPORT, strlen(k)) == 0){
        get_custom_report(v, car);
    }else if (strncmp(k, CV_API_SEAT, strlen(k)) == 0){
        if (get_nvs_value(nvs_node_number)){
            car->success = true;
            // sprintf(car->val, "%d", desired_node_number);
            //car->val = strdup(desired_node_number);
            car->val = "TODO seatNum nvs";
            car->api_code = CV_OK;
        } else {
            car->success = false;
            car->api_code = CV_ERROR_NVS_READ;
        }
    } else if (strncmp(k, CV_API_CVCM_VERSION, strlen(k)) == 0){
        get_cvcm_version(car);
    } else if (strncmp(k, CV_API_CVCM_VERSION_ALL, strlen(k)) == 0){
        get_cvcm_version_all(car);
    }else if (strncmp(k, CV_API_MAC_ADDR, strlen(k)) == 0){
        get_mac_addr(car);
    }else if (strncmp(k, CV_API_VIDEO_FORMAT, strlen(k)) == 0){
        get_videoformat(car);
    } else if (strncmp(k, CV_API_USER_MESSAGE, strlen(k)) == 0){
        car->success = false;
        car->api_code = CV_ERROR_READ;
    } else {
        CV_LOGE(TAG,"Unknown request key of '%s'",k );
        car->success = false;
        car->api_code = CV_ERROR_INVALID_ENDPOINT;
    }
}

void caw_nvs_write(struct cv_api_write* caw, char* nvs_key, char* nvs_val){
    caw->success = set_credential(nvs_key, nvs_val);
        if (caw->success){
            caw->api_code = CV_OK;
        } else {
            caw->api_code = CV_ERROR_NVS_WRITE;
        }
}

// Parse command, execute it, and return command or error as JSON
void kv_api_parse_caw(struct cv_api_write* caw, char* k, char* v) {
    char* TAG = "cv_server->kv_api_parse_caw";
    if (strncmp(k, CV_API_ADDRESS, strlen(k)) == 0){
        set_address(v, caw);
    } else if (strncmp(k, CV_API_SSID, strlen(k)) == 0){
        caw_nvs_write(caw, k, v);
    } else if (strncmp(k, CV_API_PASSWORD, strlen(k)) == 0){
        caw_nvs_write(caw, k, v);
    } else if (strncmp(k, CV_API_DEVICE_NAME, strlen(k)) == 0){
        caw_nvs_write(caw, k, v);
    } else if (strncmp(k, CV_API_BROKER_IP, strlen(k)) == 0){
        caw_nvs_write(caw, k, v);
    } else if (strncmp(k, CV_API_ANTENNA, strlen(k)) == 0){
        set_antenna(v, caw);
    } else if (strncmp(k, CV_API_BAND, strlen(k)) == 0){
        set_band(v, caw);
    } else if (strncmp(k, CV_API_CHANNEL, strlen(k)) == 0){
        set_channel(v, caw);
    } else if (strncmp(k, CV_API_ID, strlen(k)) == 0){
        set_id(v, caw);
    } else if (strncmp(k, CV_API_USER_MESSAGE, strlen(k)) == 0){
        set_usermsg(v, caw);
    } else if (strncmp(k, CV_API_VIDEO_FORMAT, strlen(k)) == 0){
        set_videoformat(v, caw);
    } else if (strncmp(k, CV_API_SEND_CMD, strlen(k)) == 0) {
        set_custom_w(v, caw);
    } else {
        CV_LOGE(TAG,"Unknown write key of '%s'",k );
        caw->success = false;
        caw->api_code = CV_ERROR_INVALID_ENDPOINT;
    }
}


static cJSON* run_kv_api(char* k, char* v){
    //char* TAG = "cv_server->run_kv_api";
    cJSON* ret = cJSON_CreateObject();
    // if it's a request, run request and return
    if (strncmp(v, CV_READ_Q ,strlen(v)) == 0 || strncmp(k,CV_API_REQ_REPORT,strlen(CV_API_REQ_REPORT)) == 0){
        struct cv_api_read car;
        struct cv_api_read*carptr = &car;
        kv_api_parse_car(carptr, k, v);
        add_response_to_json_car(ret, k, carptr);
    } else { // It's a command, write value
        struct cv_api_write caw;
        struct cv_api_write* cawptr = &caw;    
        kv_api_parse_caw(cawptr, k, v);
        add_response_to_json_caw(ret, k , cawptr, v);
        #ifdef WRITE_THEN_READ
            // If write success, then do a read and replace the value
            if (cawptr->success){
                struct cv_api_read car;
                struct cv_api_read*carptr = &car;
                vTaskDelay(1000 / portTICK_PERIOD_MS); //Delay 100ms between write and read
                kv_api_parse_car(carptr, k, CV_READ_Q);
                add_response_to_json_car(ret, k, carptr); // TODO is the response added or modified?
            }
        #endif
    }
    return ret;
}

// Take JSON formatted cmd and return a JSON response
// Supports multiple cJSON elements and will return JSON for each
static cJSON* run_json_api(cJSON* obj)
{
    cJSON* retJSON = NULL; //the return val
    cJSON *element = NULL;
    char *k = NULL;
    char *v = NULL;
    if (cJSON_GetArraySize(obj)){
        cJSON_ArrayForEach(element, obj)
        {
            k = element->string;
            v = element->valuestring;

            if (k != NULL && v != NULL)
            {
                ESP_LOGI("run_api", "key=%s,val=%s", k, v);
                // cJSON_AddItemToObject()
                cJSON* kj = run_kv_api(k, v);
                retJSON = cJSONUtils_MergePatchCaseSensitive(retJSON,kj);
            }
        }
    } else {
        ESP_LOGW(TAG_SERVER, "JSON EMPTY");
    }
    return retJSON;
}

// static esp_err_t config_test_post_handler(httpd_req_t *req)
// {
//     char buf[100]; // TODO check expected behavior for buffer overflow
//     char val[64];
//     int ret, remaining = req->content_len;
    
//     while (remaining > 0) {
//         /* Read the data for the request */
//         if ((ret = httpd_req_recv(req, buf,
//                         MIN(remaining, sizeof(buf)))) <= 0) {
//             if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//                 /* Retry receiving if timeout occurred */
//                 continue;
//             }
//             return ESP_FAIL;
//         }
        
//         if (is_content_json(req) == 0) {
//             ESP_LOGI(TAG_SERVER, "Content is json");
//         } else {
//             ESP_LOGI(TAG_SERVER, "Content not json");
//         }

//         if (httpd_query_key_value(buf, "UM", val, sizeof(val)) == ESP_OK) {
//             remove_ctrlchars(val);
//             ESP_LOGD(TAG_SERVER, "Setting user message string to: %s", val);
//             set_usermsg(val);
//             memset(&val[0], 0, sizeof(val));
//         } 
//         else if (httpd_query_key_value(buf, "RL", val, sizeof(val)) == ESP_OK) {
//             ESP_LOGD(TAG_SERVER, "Found URL query parameter => Request Lock Status"); //unused param
//             uint8_t* dataRx = (uint8_t*) malloc(RX_BUF_SIZE+1);
//             int repCount = cvuart_send_report(LOCK_REPORT_FMT, dataRx);
//             if (repCount > 0){
//                 remove_ctrlchars((char*)dataRx);
//                 httpd_resp_send_chunk(req, (char*)dataRx, HTTPD_RESP_USE_STRLEN);
//             } else if (repCount == 0 ) {
//                 httpd_resp_send_chunk(req, "No uart response", HTTPD_RESP_USE_STRLEN);
//             } else if (repCount == -1) {
//                 httpd_resp_send_chunk(req, "Error: uart disabled", HTTPD_RESP_USE_STRLEN);
//             }
//                 // cvuart_send_report("\n09RPMV%%\r", dataRx);
//                 // cvuart_send_report("\n09RPVF%%\r", dataRx);
//                 //cvuart_send_report("\n09RPID%%\r", dataRx);
//                 //run_cv_uart_test_task();
//             free(dataRx);
//         } 
//         else if (httpd_query_key_value(buf, "LED", val, sizeof(val)) == ESP_OK) {
    
//             #if CONFIG_ENABLE_LED
//                 if ((strncmp(val, "ON", 2)) == 0){
//                     httpd_resp_send_chunk(req, "LED ON", HTTPD_RESP_USE_STRLEN);
//                     set_ledc_code(0, led_on);
//                 } else if ((strncmp(val, "OFF", 3)) == 0){
//                     httpd_resp_send_chunk(req, "LED OFF", HTTPD_RESP_USE_STRLEN);
//                     set_ledc_code(0, led_off);
//                 } else {
//                     ESP_LOGE(TAG_SERVER, "Unsupported LED value of %s", val);
//                     httpd_resp_send_chunk(req, "Error: unknown led state", HTTPD_RESP_USE_STRLEN);
//                 }
//             #else //not CONFIG_ENABLE_LED
//                 httpd_resp_send_chunk(req, "Error: LED is disabled", HTTPD_RESP_USE_STRLEN);
//             #endif //CONFIG_ENABLE_LED
//         }
//         else {
//             ESP_LOGW(TAG_SERVER, "Unknown param in %s", buf);
//             httpd_resp_set_status(req, HTTPD_400);
//             httpd_resp_send_chunk(req, "Error: Invalid API Endpoint<br>", HTTPD_RESP_USE_STRLEN);
//             httpd_resp_send_chunk(req, buf, ret);
//             httpd_resp_send_chunk(req, NULL, 0);
//             return ESP_OK;
//         }

//         //Redirect
//         httpd_resp_send_chunk(req, "<head>", HTTPD_RESP_USE_STRLEN); 
//         char* redirect_str = "<meta http-equiv=\"Refresh\" content=\"3; URL=http://192.168.4.1/test\">";
//         httpd_resp_send_chunk(req, redirect_str, HTTPD_RESP_USE_STRLEN);
//         httpd_resp_send_chunk(req, "</head>", HTTPD_RESP_USE_STRLEN);  
//         httpd_resp_send_chunk(req, "Redirecting in 3s. <br>", HTTPD_RESP_USE_STRLEN);

        

//         /* Send back the same data */
//         httpd_resp_send_chunk(req, buf, ret);

//         /* Log data received */
//         ESP_LOGI(TAG_SERVER, "=========== RECEIVED DATA ==========");
//         ESP_LOGI(TAG_SERVER, "%.*s", ret, buf);
//         ESP_LOGI(TAG_SERVER, "====================================");
//         remaining -= ret;
    

//     }
//     // End response
//     httpd_resp_send_chunk(req, NULL, 0);
//     return ESP_OK;
// }


static esp_err_t config_settings_post_handler(httpd_req_t *req)
{
    ESP_LOGD(TAG_SERVER, "Got config_settings request");
        /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */
    char buf[POST_BUFFER_SZ]; //TODO optimize size based on 32char
    
    bool success = true;

    /* Truncate if content length larger than the buffer */
    if (req->content_len >= sizeof(buf)){
        ESP_LOGE(TAG_SERVER, "Content too large. size '%d' > '%d'", req->content_len, sizeof(buf));
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_send(req, "Content size exceeded", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    size_t recv_size = MIN(req->content_len, sizeof(buf));
    //TODO warn if content too large

    int ret = httpd_req_recv(req, buf, recv_size);
    // printf("\nContent:'%s', ret '%d'\n", buf, ret);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        char* err = "{\"Error=content\":\"0\"}";
        httpd_resp_send_chunk(req,err , HTTPD_RESP_USE_STRLEN);
        httpd_resp_send_chunk(req, NULL, 0);
        ESP_LOGW(TAG_SERVER, "Content Length 0 or other issue");
        
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    if (is_content_json(req) == 0) { //Content is json
        ESP_LOGD(TAG_SERVER, "Content is json");
        // TODO parse JSON here
                // TODO parse JSON here
        cJSON *json_post = cJSON_Parse(buf);
        if (json_post == NULL) {
            ESP_LOGE(TAG_SERVER, "JSON Parse Failure");
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                ESP_LOGI(TAG_SERVER, "JSON Parse Error starting at: %s\n", error_ptr);
                ESP_LOGI(TAG_SERVER, "Full JSON: %s", buf);
            }
            httpd_resp_send_chunk(req, "{\"error\":\"error-json-parse\"}",HTTPD_RESP_USE_STRLEN);
            success = false;
        } else {
            char* cjson_arrived = cJSON_Print(json_post);
            
            printf("JSON In: %s\n", cjson_arrived);
            cJSON *json_returned = run_json_api(json_post); //TODO memory leak?
            if (json_returned){
                char* cjson_returned = cJSON_Print(json_returned);
                printf("JSON Back: %s\n", cjson_returned);
                ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Content-Type",HTTPD_TYPE_JSON));
                httpd_resp_send_chunk(req, cjson_returned, HTTPD_RESP_USE_STRLEN);  
                free(cjson_returned);
            }
            free(cjson_arrived);
            
        }
        

        cJSON_Delete(json_post);
    } else {
        ESP_LOGW(TAG_SERVER, "Content not json - ignore parsing");
        ESP_LOGI(TAG_SERVER, "buf:'%s'",buf);
        httpd_resp_set_status(req, "406 Not Acceptable");
        const char CSZ = 50;
        char content_type[CSZ];
        httpd_req_get_hdr_value_str(req, "Content-Type", content_type, CSZ);
        char content_msg[110]; //50+60
        sprintf(content_msg,"Error: header content-type must be 'application/json', not '%s'", content_type );

        httpd_resp_send(req, content_msg, HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
        success = false;
        //char val[64];
        //Loop through the key and values. 


        // Desired: run_kv_api(char* k, char* v)
        // if (httpd_query_key_value(buf, "address", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting address to: %s", val);
        //     set_address(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "antenna_mode", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting antenna mode to: %s", val);
        //     set_antenna(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "channel", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting channel to: %s", val);
        //     set_channel(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "band", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting band to: %s", val);
        //     set_band(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "id", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting user id string to: %s", val);
        //     set_id(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "user_message", val, sizeof(val)) == ESP_OK) {
        //     ESP_LOGI(TAG_SERVER, "BUF: '%s'", buf);
        //     ESP_LOGI(TAG_SERVER, "Before CTL: '%s'", val);
        //     remove_ctrlchars(val);
        //     ESP_LOGI(TAG_SERVER, "Setting user message string to: '%s'", val);
        //     set_usermsg(val);
        //     printf("\nAPI LENGTH %d\n", strlen(val));
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "mode", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting mode to: %s", val);
        //     set_mode(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "osd_visibility", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting osd visibility to: %s", val);
        //     set_osdvis(val);
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "osd_position", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Setting osd position to: %s", val);
        //     set_osdpos(val);
        //     memset(&val[0], 0, sizeof(val));
        // }
        // else if (httpd_query_key_value(buf, "reset_lock", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGD(TAG_SERVER, "Resetting Lock; unused val: %s", val);
        //     reset_lock();
        //     memset(&val[0], 0, sizeof(val));
        // } 
        // else if (httpd_query_key_value(buf, "video_format", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGI(TAG_SERVER, "Setting video format to: %s", val);
        //     if (strcmp(val, "Auto") == 0)set_videoformat("A");
        //     else if (strcmp(val, "NTSC") == 0)set_videoformat("N");
        //     else if (strcmp(val, "PAL") == 0)set_videoformat("P");
        //     memset(&val[0], 0, sizeof(val));
        // }
        // else if (httpd_query_key_value(buf, "send_cmd", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGI(TAG_SERVER, "Sending custom cmd: %s", val);
        //     set_custom_w(val);
        //     memset(&val[0], 0, sizeof(val));
        // }    
        // else if (httpd_query_key_value(buf, "send_rep", val, sizeof(val)) == ESP_OK) {
        //     remove_ctrlchars(val);
        //     ESP_LOGI(TAG_SERVER, "Sending custom report: %s", val);
        //     struct cv_api_read* car = get_custom_report(val);
        //     printf("Response: %s", car->val);
        //     memset(&val[0], 0, sizeof(val));
        // }  
        // else {
        //     ESP_LOGW(TAG_SERVER, "config_settings_post:Unknown API endpoint in %s", buf);
        //     httpd_resp_set_status(req, HTTPD_400);
        //     httpd_resp_send_chunk(req, "Error: Invalid API Endpoint<br>", HTTPD_RESP_USE_STRLEN);
        //     httpd_resp_send_chunk(req, buf, ret);
        //     httpd_resp_send_chunk(req, NULL, 0);
        //     return ESP_OK;
        // }
    }


    if (success){
        //Redirect
        // httpd_resp_send_chunk(req, "<head>", HTTPD_RESP_USE_STRLEN); 
        // char* redirect_str = "<meta http-equiv=\"Refresh\" content=\"0; URL=http://192.168.4.1/settings\">";
        // httpd_resp_send_chunk(req, redirect_str, HTTPD_RESP_USE_STRLEN);
        // httpd_resp_send_chunk(req, "</head>", HTTPD_RESP_USE_STRLEN);  
        // httpd_resp_send_chunk(req, "Redirecting in 3s. TODO verify data was set in CV <br>", HTTPD_RESP_USE_STRLEN);    
        ESP_LOGD(TAG_SERVER, "Parse Success: '%s'", buf);
    } 

    



    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// static esp_err_t config_wifi_post_handler(httpd_req_t *req)
// {

//     char buf[100]; // TODO check expected behavior for buffer overflow
//     char val[64];
//     int ret, remaining = req->content_len;
    
//     while (remaining > 0) {
//         /* Read the data for the request */
//         if ((ret = httpd_req_recv(req, buf,
//                         MIN(remaining, sizeof(buf)))) <= 0) {
//             if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
//                 /* Retry receiving if timeout occurred */
//                 continue;
//             }
//             return ESP_FAIL;
//         }
//         remaining -= ret;
//     }

//     char* tok;
//     char * search = "\r\n";
//     tok = strtok(buf, search);

//     while ( tok != NULL){
//         printf("Tok: '%s'\n", tok);
//         if (httpd_query_key_value(tok, "ssid", val, sizeof(val)) == ESP_OK) {
//             remove_ctrlchars(val);
//             ESP_LOGI(TAG_SERVER, "Found URL query valeter => ssid=%s", val);
//             ssid_ready = true; //TODO state machine set false
//             set_credential("ssid", val);
//             memset(&val[0], 0, sizeof(val));
//         }
//         if (httpd_query_key_value(tok, "password", val, sizeof(val)) == ESP_OK) {
//             remove_ctrlchars(val);
//             ESP_LOGI(TAG_SERVER, "Found URL query parameter => password=%s", val);
//             password_ready = true;
//             set_credential("password", val);
//             memset(&val[0], 0, sizeof(val));
//         }
//         if (httpd_query_key_value(tok, "device_name", val, sizeof(val)) == ESP_OK) {
//             remove_ctrlchars(val);
//             ESP_LOGI(TAG_SERVER, "Found URL query parameter => device_name=%s", val);
//             device_name_ready = true;
//             set_credential("device_name", val);
//             memset(&val[0], 0, sizeof(val));
//         }
//         if (httpd_query_key_value(tok, "broker_ip", val, sizeof(val)) == ESP_OK) {
//             remove_ctrlchars(val);
//             ESP_LOGI(TAG_SERVER, "Found URL query parameter => broker_ip=%s", val);
//             broker_ip_ready = true;
//             set_credential("broker_ip", val);
//             memset(&val[0], 0, sizeof(val));
//         }
//         if (httpd_query_key_value(tok, "seat_number", val, sizeof(val)) == ESP_OK) {
//                 remove_ctrlchars(val);
//                 ESP_LOGI(TAG_SERVER, "Setting seat number to: %s", val);
//                 if (set_credential("node_number", val)){
//                     update_subscriptions_new_node();
//                 }
//                 memset(&val[0], 0, sizeof(val));
//         } 
//         tok = strtok(NULL, search);
//     }


    


//     if (ssid_ready && password_ready && device_name_ready && broker_ip_ready){
//         //Match the URI to determine whether to send instantly or not
//         printf("%s\n", req->uri);
//         bool is_instant = false;
//         bool is_matched = false;

//         bool uri_match;
//         uri_match = httpd_uri_match_wildcard(CV_CONFIG_WIFI_URI, req->uri, strlen(CV_CONFIG_WIFI_URI));
//         if (uri_match) {
//             is_instant = false;
//             is_matched = true;
//         }
//         uri_match = httpd_uri_match_wildcard(CV_CONFIG_WIFI_INSTANT_URI, req->uri, strlen(CV_CONFIG_WIFI_INSTANT_URI));;
//         if (uri_match) {
//                         is_instant = false;
//                         is_matched = true;
//         }
//         if (!is_matched){
//             //ESP_LOGI(TAG_SERVER, LOG_FMT("URI '%s' unmatched"), req->uri);
//             ESP_LOGE(TAG_SERVER, "Unmatched URI");
//         }

//         /*
//         // TODO add in content in the response to say "form submitted".
//         // Redirect both address immediately to "/wifi_config_saved". Save _is_instant. 
//         // Set up /wifi_config_saved to serve the saved wifi settings. 
//         // Try encoding the data as a json file encoding using the HTML headers */

//         if (is_instant){
//             ESP_LOGI(TAG_SERVER, "All WiFi Creds received. Configuring now");
//             // HTML Redirect https://developer.mozilla.org/en-US/docs/Web/HTTP/Redirections
//             // httpd_resp_send_chunk(req, "<head>", HTTPD_RESP_USE_STRLEN); 
//             // char* redirect_str = "<meta http-equiv=\"Refresh\" content=\"0; URL=http://192.168.4.1/\">";
//             // httpd_resp_send_chunk(req, redirect_str, HTTPD_RESP_USE_STRLEN);
//             // httpd_resp_send_chunk(req, "</head>", HTTPD_RESP_USE_STRLEN);     
//             // httpd_resp_send_chunk(req, NULL, 0);
//             // vTaskDelay(1000 / portTICK_PERIOD_MS);
//         } else {
//             ESP_LOGI(TAG_SERVER, "All WiFi Creds received. Sending Delayed Redirect");
//             // httpd_resp_send_chunk(req, "<head>", HTTPD_RESP_USE_STRLEN); 
//             // char* redirect_str = "<meta http-equiv=\"Refresh\" content=\"3; URL=http://192.168.4.1/\">";
//             // httpd_resp_send_chunk(req, redirect_str, HTTPD_RESP_USE_STRLEN);
//             // httpd_resp_send_chunk(req, "</head>", HTTPD_RESP_USE_STRLEN);     
//             // httpd_resp_send_chunk(req, NULL, 0);
//             //vTaskDelay(6000 / portTICK_PERIOD_MS);
//         }
        
//         // TODO the redirect may be causing an issue as far as timing and switching out of the ode
//         // Maybe this should return with no delay? 
//         /* 
//         W (164882) httpd_txrx: httpd_sock_err: error in send : 113
//         ESP_ERROR_CHECK failed: esp_err_t 0xb006 (ERROR) at 0x40090df4
//         0x40090df4: _esp_error_check_failed at /home/ryan/esp/esp-idf/components/esp32/panic.c:726

//         file: "../main/cv_server.c" line 208
//         func: serve_title
//         expression: httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN)
//         */
        
//         // HTTP Redirect Method
//         // https://esp32.com/viewtopic.php?t=11012
//         // httpd_resp_set_type(req, "text/html");
//         // httpd_resp_set_status(req, "307 Temporary Redirect");
//         // httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/wifi_confirmation");

//         //Redirect
//         httpd_resp_send_chunk(req, "<head>", HTTPD_RESP_USE_STRLEN); 
//         char* redirect_str = "<meta http-equiv=\"Refresh\" content=\"3; URL=http://192.168.4.1/\">";
//         httpd_resp_send_chunk(req, redirect_str, HTTPD_RESP_USE_STRLEN);
//         httpd_resp_send_chunk(req, "</head>", HTTPD_RESP_USE_STRLEN);  
//         httpd_resp_send_chunk(req, "Wifi Configuring. <br>", HTTPD_RESP_USE_STRLEN);

//         httpd_resp_send(req, NULL, 0);
//         vTaskDelay(1000 / portTICK_PERIOD_MS);
//         extern bool switch_to_sta;
//         switch_to_sta = true;
//     } else {
//         httpd_resp_send(req, NULL, 0);
//     }

//     return ESP_OK;
// }

void serve_title(httpd_req_t *req) {
    char chipid[UNIQUE_ID_LENGTH];
    get_chip_id(chipid, UNIQUE_ID_LENGTH);

    // Allocate memory for the format string
    size_t n_template = title_html_end - title_html_start;
    char* line_template = (char*)malloc(n_template);
    snprintf(line_template, n_template, "%s", title_html_start);

    // Allocate memory for the output string
    size_t needed = snprintf(NULL, 0, line_template, chipid)+1;
    char* line = (char*)malloc(needed);
    snprintf(line, needed, line_template, chipid);
    free(line_template);
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN));
    free(line);
}

void serve_node_and_lock(httpd_req_t *req){
    extern uint8_t desired_node_number;
    char* lock_status = "TODO";

    // Allocate memory for the format string
    size_t n_template = subtitle_html_end - subtitle_html_start;
    char* line_template = (char*)malloc(n_template);
    snprintf(line_template, n_template, "%s", subtitle_html_start);

    // Display the node number 1 higher than stored in the back end. 
    // Back end
    int ui_node_number = desired_node_number+1;
    size_t needed = snprintf(NULL, 0, line_template, ui_node_number, lock_status)+1;
    char* line = (char*)malloc(needed);
    snprintf(line, needed, line_template, ui_node_number, lock_status);
    free(line_template);
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN));
    free(line);
    //ESP_ERROR_CHECK(httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN));
}

void serve_wificonfig(httpd_req_t *req){
    // Allocate memory for the format string
    size_t n_template = wifiSettings_html_end - wifiSettings_html_start;
    char* line_template = (char*)malloc(n_template);
    snprintf(line_template, n_template, "%s", wifiSettings_html_start);

    size_t needed = snprintf(NULL, 0, line_template, desired_ap_ssid, desired_ap_pass, desired_friendly_name, desired_mqtt_broker_ip)+1;
    char* line = (char*)malloc(needed);
    snprintf(line, needed, line_template, desired_ap_ssid, desired_ap_pass, desired_friendly_name, desired_mqtt_broker_ip);
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN));
    free(line);
}

void serve_settingsconfig(httpd_req_t *req){
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)settings_html_start, settings_html_end - settings_html_start));
}

void serve_test(httpd_req_t *req){
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)test_html_start, test_html_end - test_html_start));
}

void serve_html_beg(httpd_req_t *req){
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)html_beg_start, html_beg_end - html_beg_start));
}

void serve_html_end(httpd_req_t *req){
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)html_conc_start, html_conc_end - html_conc_start));
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, NULL, 0));
}

void serve_menu_bar(httpd_req_t *req){
    ESP_ERROR_CHECK(httpd_resp_send_chunk(req, (const char *)menu_bar_start, menu_bar_end - menu_bar_start));
}

static esp_err_t wifi_get_handler(httpd_req_t *req)
{
    serve_html_beg(req);
    serve_title(req);
    serve_node_and_lock(req);
    serve_menu_bar(req);
    serve_wificonfig(req);
    serve_html_end(req);
    return ESP_OK;
}


static esp_err_t settings_get_handler(httpd_req_t *req)
{
    serve_html_beg(req);
    serve_title(req);
    serve_node_and_lock(req);
    serve_menu_bar(req);
    serve_settingsconfig(req);
    serve_html_end(req);
    return ESP_OK;
}

static esp_err_t test_cv_get_handler(httpd_req_t *req)
{   
    serve_html_beg(req);
    serve_title(req);
    serve_menu_bar(req);
    serve_test(req);
    serve_html_end(req);
    return ESP_OK;
}

/* jquery GET handler */
static esp_err_t cv_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG_SERVER, "cv_server requested");

	httpd_resp_set_type(req, "application/javascript");

	httpd_resp_send(req, (const char *)cv_js_start, (cv_js_end - cv_js_start)-1);

	return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = wifi_get_handler
};

static const httpd_uri_t wifi_uri = {
    .uri       = "/wifi_get",
    .method    = HTTP_GET,
    .handler   = wifi_get_handler
};

static const httpd_uri_t settings_uri = {
    .uri       = "/settings",
    .method    = HTTP_GET,
    .handler   = settings_get_handler
};

static const httpd_uri_t config_settings_uri = {
    .uri       = "/settings",
    .method    = HTTP_POST,
    .handler   = config_settings_post_handler
};

// // When a user manually submits wifi config. Issues a confirmation page
// static const httpd_uri_t config_wifi_uri = {
//     .uri       = "/wifi_config",
//     .method    = HTTP_POST,
//     .handler   = config_wifi_post_handler
// };

// Used to submit wifi credentials without promting or redirecting
// static const httpd_uri_t config_wifi_instant_uri = {
//     .uri   = "/wifi_instant",
//     .method    = HTTP_POST,
//     .handler   = config_wifi_post_handler
// };


static const httpd_uri_t test_uri = {
    .uri       = "/test",
    .method    = HTTP_GET,
    .handler   = test_cv_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
};

// static const httpd_uri_t config_test_uri = {
//     .uri       = "/test",
//     .method    = HTTP_POST,
//     .handler   = config_test_post_handler
// };


static const httpd_uri_t cv_js_uri = {
	.uri = "/cv_js.js",
	.method = HTTP_GET,
	.handler = cv_js_handler,
	/* Let's pass response string in user
	 * context to demonstrate it's usage */
	.user_ctx = NULL
};

extern httpd_handle_t start_cv_webserver(void){
    if (_server_started) {
        ESP_LOGW(TAG_SERVER, "SERVER is already running");
        return NULL;
    }


    esp_log_level_set(TAG_SERVER, ESP_LOG_DEBUG);
    //start the OTA reboot task
    xTaskCreate(&otaRebootTask, "rebootTask", 2048, NULL, 5, NULL);

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Lets bump up the stack size (default was 4096)
	config.stack_size = 8192;
    config.max_uri_handlers = 14;
    ESP_LOGI(TAG_SERVER, "Starting server on port: '%d'", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    ESP_ERROR_CHECK(ret); // this should bail if the server can't start
    // Start the httpd server
    ESP_LOGI(TAG_SERVER, "Server started");
    
    if (ret == ESP_OK){
        // Set URI handlers
        ESP_LOGI(TAG_SERVER, "Registering URI handlers");

        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_uri));
        //ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config_wifi_uri));
        //ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config_wifi_instant_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config_settings_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settings_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &OTA_index_html));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &OTA_favicon_ico));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &OTA_jquery_3_4_1_min_js));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &OTA_update));
		ESP_ERROR_CHECK(httpd_register_uri_handler(server, &OTA_status));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &cv_js_uri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &test_uri));
        //ESP_ERROR_CHECK(httpd_register_uri_handler(server, &config_test_uri));


        ESP_LOGI(TAG_SERVER, "Webserver started. Login and go to http://192.168.4.1/ \n");
        _server_started = true;
        return server;
    }

    ESP_LOGI(TAG_SERVER, "Error starting server!");
    _server_started = false;
    return NULL;
}


static void stop_cv_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
    _server_started = false;
}

//TODO link the disconnect and connect handlers to joinging/leaving the wifi?

static void cv_disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG_SERVER, "Stopping webserver");
        stop_cv_webserver(*server);
        *server = NULL;
    }
}

static void cv_connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG_SERVER, "Starting webserver");
        *server = start_cv_webserver();
    }
}

#endif //CV_SERVER_C