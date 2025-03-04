#ifndef STRUCTS_AND_DEFS
#define STRUCTS_AND_DEFS


#include <cstdint>
#include <arpa/inet.h>
#include <optional>


#pragma pack(push, 1)
typedef struct {
    uint16_t size;
    uint16_t protocol : 12;
    uint8_t addressable : 1;
    uint8_t tagged : 1;
    uint8_t reserved_1 : 2;
    uint32_t source;
    uint64_t target;
    uint8_t reserved_2[6];
    uint8_t res_required : 1;
    uint8_t ack_required : 1;
    uint8_t reserved_3 : 6;
    uint8_t sequence;
    uint64_t reserved_4;
    uint16_t pkt_type;
    uint16_t reserved_5;
} lx_frame_t;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct stateService {
    uint8_t service;
    uint32_t port;
} stateService;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct setLightPower {
    uint16_t level;         // The power level can be either  (0) or enabled (65535)
    uint32_t duration;      // The duration is the power level transition time in milliseconds
} setLightPower;   
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct setDevicePower {
    uint16_t level;         // 0 turns light off and puts in standby mode where it uses less power and ownt respond as quickly, 65535 take sit out of that state
} setDevicePower;   
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct lxDevice {
    uint8_t service;
    uint32_t port;
    uint64_t target;
    struct in_addr address;
    uint32_t source;    // stays the same dont set to 0 or 1, same for all lights
    uint8_t sequence;   // incremented each time message is sent to device, device reponds with same number
} lxDevice;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct stateVersion {
    uint32_t vendor;
    uint32_t product;
    uint32_t version;
} stateVersion;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct deviceInfo{ 
    const char *name; 
    int product; 
    const char *color; 
    bool infrared; 
    const char *zones; 
    int kelvin_min; 
    int kelvin_max; 
} deviceInfo; 
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct Response{
    const char* content;
    ssize_t size;
    uint16_t message_type;
} Response; 
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct statePower{
    uint16_t level;
} statePower;
#pragma pack(pop)


// placeholder for template 
typedef struct nullPtr{
} nullPtr;



#pragma pack(push, 1)
typedef struct HSBK{
    uint16_t hue;
    uint16_t saturation;
    uint16_t brightness;
    uint16_t kelvin;
} HSBK;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct state{
    struct HSBK color;
    uint16_t reserved;
    uint16_t power;
    char label[32];
    uint64_t reserved2;
} state;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct setColor{
    uint8_t reserved;
    struct HSBK color;
    uint32_t duration;
} setColor;
#pragma pack(pop)


enum multiZoneApplicationRequest {NO_APPLY, APPLY, APPLY_ONLY};


#pragma pack(push, 1)
typedef struct setColorZones{
    uint8_t start_index;
    uint8_t end_index;
    uint16_t hue;
    uint16_t saturation;
    uint16_t brightness;
    uint16_t kelvin;
    uint32_t duration;
    uint8_t apply;
} setColorZones;
#pragma pack(pop)




#pragma pack(push, 1)
typedef struct setExtendedColorZones{
    uint32_t duration;
    uint8_t apply;
    uint16_t zone_index;
    uint8_t color_count;
    struct HSBK colors[82];
} setExtendedColorZones;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct stateMultiZone{
    uint8_t zones_count;
    uint8_t zone_index;
    struct HSBK colors[8];
} stateMultiZone;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct stateExtendedColorZones{
    uint16_t zones_count;
    uint16_t zone_index;
    uint8_t colors_count;
    struct HSBK colors[82];
} stateExtendedColorZones;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct getColorZones{
    uint8_t start_index;
    uint8_t end_index;

} getColorZones;
#pragma pack(pop)



#pragma pack(push, 1)
typedef struct getExtendedColorZones{
    uint16_t count;
    uint16_t index;
    uint8_t zones_count;
    
} getExtendedColorZones;
#pragma pack(pop)



#pragma pack(push, 1)
typedef  struct stateZone{
    uint8_t zone_count;
    uint8_t index;
    uint16_t hue;
    uint16_t saturation;
    uint16_t brightness;
    uint16_t kelvin;

} stateZone;
#pragma pack(pop)






#pragma pack(push, 1)
typedef struct stateHostFirmware{
    uint64_t build;
    uint64_t reserved;
    uint32_t version;  
} stateHostFirmware;
#pragma pack(pop)




typedef struct extendedDeviceInfo {
    const char *name;
    uint32_t pid;
    uint32_t vid; 
    uint16_t firmwareMinor;
    uint16_t firmwareMajor;
    bool hev = false;
    bool color = false;
    bool chain = false;
    bool matrix = false;
    bool relays = false;
    bool buttons = false;
    bool infrared = false;
    bool multizone = false;
    bool extended_multizone = false;
    std::optional<uint16_t> tempLowRange;
    std::optional<uint16_t> tempUpRange;
} extendedDeviceInfo;




typedef struct inputArgs {

    bool help = false;
    bool on = false;
    bool off = false;
    bool setCol = false;
    bool list = false;
    bool info = false;
    bool brightness = false;
    bool warmth = false;
    bool setCols = false;
    bool setColsR = false;
    bool ip = false;
    bool all = false;
    bool duration = false;
    bool listScene = false;
    bool loadScene = false;
    bool saveScene = false;


    HSBK* color;// for setCol
    uint32_t address = 0; // for ip and info
    uint16_t brightVal = 0; // %0-100
    int kelvin; // between 1500 - 9000 (general)
    std::vector<HSBK*>* colsVec; // for setCols 
    std::vector<HSBK*>* colsRVec; // for setColsR
    uint32_t durVal = 0;
    std::string oldScene;
    std::string newScene;



} inputArgs;






// lifx messages
#define GET_SERVICE 2
#define DEFAULT_LIFX_PORT 56700
#define BUFFER_SIZE 1024
#define LIGHT_SET_POWER 117  // either 0 for off or 65535 for on
#define STATE_POWER_LIGHT 118 
#define GET_LABEL 23
#define STATE_LABEL 25
#define GET_VERSION 32
#define STATE_VERSION 33
#define STATE_VERSION_LENGTH 48
#define HEADER_SIZE 36
#define GET_POWER 20
#define STATE_POWER_DEVICE 22
#define GET 101
#define STATE 107
#define DEVICE_SET_POWER 21     // uint16 so set the power 
#define SET_COLOR 102
#define SET_COLOR_ZONES 501
#define SET_EXTENDED_COLOR_ZONES 510
#define STATE_MULTI_ZONE 506
#define STATE_EXTENDED_COLOR_ZONES 512
#define GET_COLOR_ZONES 502
#define GET_EXTENDED_COLOR_ZONES 511
#define STATE_ZONE 503
#define STATE_SERVICE 3
#define SET_COLOR 102
#define ACKNOWLEDGEMENT 45
#define STATE_HOST_FIRMWARE 15
#define GET_HOST_FIRMWARE 14







extern uint32_t SOURCE;
const std::string PATH_TO_SCENES = "scenes/";
const std::string PATH_TO_PRODUCTS = "data/products.json";













#endif // STRUCTS_AND_DEFS
