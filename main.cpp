// ************   Feature To Add   **************
// add to the toHS funciton that it gets the current brightness and sets that
// add default colors
// adding multi response sendPacket so can capture multizone info to save scenes
// instead of using getVersion and lookin up for listDevices use get and use label and only loopup when -info is called
// cut down the ammount of time it takes to get a response
// make ack_reauired on all the time and add acknowledgement to all respose types incase some firmwares dont response to certain messages

// ***********    Bugs to Fix      **************
// buffering on legacy multiZone changes not wokring as expected


#include <iostream>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <ifaddrs.h>
#include <stdio.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <math.h>
#include <algorithm> 
#include <random>


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





uint32_t SOURCE;
char label[32];              // value returned by StateLabel message



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



void printHelp();
std::string uint64ToMac(uint64_t target);
bool listDevices();
bool listDevicesOld();
const deviceInfo* getDeviceInfo(int productId);
std::vector<in_addr> getAllDevices();
bool isInParams(int count, char** params, std::string test);
bool setPower(bool braodcast, bool brightness, uint32_t ip, int level, int durtaion);
char* getNextParam(int count, char** params, std::string test);
bool printInfo(bool broadcast, uint32_t ip);
HSBK* toHS(char* hexOrRGb);
bool setColorF(bool broadcast, uint32_t ip, int duration, char* color);
void printHelp();
bool setKelvin(bool broadcast, uint32_t ip, int duration, uint16_t kelvin);
bool convertToUint16(const char* str, uint16_t& result);
bool setColorZonesF(bool braodcast, uint32_t ip, uint32_t duration, char** coloArr);
std::vector<char*> parseColorList(char* list);





const deviceInfo lxDevs[] = { 
    
    {"LIFX Original 1000", 1, "Full Color", false, "Single", 2500, 9000},
    {"LIFX Color 650", 3, "Full Color", false, "Single", 2500, 9000},
    {"LIFX White 800 (Low Voltage)", 10, "Warm to White", false, "Single", 2700, 6500},
    {"LIFX White 800 (High Voltage)", 11, "Warm to White", false, "Single", 2700, 6500},
    {"LIFX Color 1000", 15, "Full Color", false, "Single", 2500, 9000},
    {"LIFX White 900 BR30 (Low Voltage)", 18, "Warm to White", false, "Single", 2500, 9000},
    {"LIFX White 900 BR30 (High Voltage)", 19, "Warm to White", false, "Single", 2500, 9000},
    {"LIFX Color 1000 BR30", 20, "Full Color", false, "Single", 2500, 9000},
    {"LIFX Color 1000", 22, "Full Color", false, "Single", 2500, 9000},
    {"LIFX A19", 27, "Full Color", false, "Single", 2500, 9000},
    {"LIFX BR30", 28, "Full Color", false, "Single", 2500, 9000},
    {"LIFX A19 Night Vision", 29, "Full Color", true, "Single", 2500, 9000},
    {"LIFX BR30 Night Vision", 30, "Full Color", true, "Single", 2500, 9000},
    {"LIFX Z", 31, "Full Color", false, "Linear", 2500, 9000},
    {"LIFX Z", 32, "Full Color", false, "Extended Linear", 2500, 9000},
    {"LIFX Downlight", 36, "Full Color", false, "Single", 2500, 9000},
    {"LIFX Downlight", 37, "Full Color", false, "Single", 2500, 9000},
    {"LIFX Beam", 38, "Full Color", false, "Extended Linear", 2500, 9000},
    {"LIFX Downlight White to Warm", 39, "Warm to White", false, "Single", 2500, 9000},
    {"LIFX Downlight", 40, "Full Color", false, "Single", 2500, 9000},
    {"LIFX A19", 43, "Full Color", false, "Single", 2500, 9000},
    {"LIFX BR30", 44, "Full Color", false, "Single", 2500, 9000},
    {"LIFX A19 Night Vision", 45, "Full Color", true, "Single", 2500, 9000},
    {"LIFX BR30 Night Vision", 46, "Full Color", true, "Single", 2500, 9000},
    {"LIFX Mini Color", 49, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Mini White to Warm", 50, "Warm to White", false, "Single", 1500, 6500},
    {"LIFX Mini White", 51, "White", false, "Single", 2700, 2700},
    {"LIFX GU10", 52, "Full Color", false, "Single", 1500, 9000},
    {"LIFX GU10", 53, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Tile", 55, "Full Color", false, "Matrix (with chain)", 2500, 9000},
    {"LIFX Candle", 57, "Full Color", false, "Matrix", 1500, 9000},
    {"LIFX Mini Color", 59, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Mini White to Warm", 60, "Warm to White", false, "Single", 1500, 6500},
    {"LIFX Mini White", 61, "White", false, "Single", 2700, 2700},
    {"LIFX A19", 62, "Full Color", false, "Single", 1500, 9000},
    {"LIFX BR30", 63, "Full Color", false, "Single", 1500, 9000},
    {"LIFX A19 Night Vision", 64, "Full Color", true, "Single", 1500, 9000},
    {"LIFX BR30 Night Vision", 65, "Full Color", true, "Single", 1500, 9000},
    {"LIFX Mini White", 66, "White", false, "Single", 2700, 2700},
    {"LIFX Candle", 68, "Full Color", false, "Matrix", 1500, 9000}, 
    {"LIFX Candle White to Warm", 81, "Warm to White", false, "Single", 2200, 6500},
    {"LIFX Filament Clear", 82, "White", false, "Single", 2100, 2100},
    {"LIFX Filament Amber", 85, "White", false, "Single", 2000, 2000},
    {"LIFX Mini White", 87, "White", false, "Single", 2700, 2700},
    {"LIFX Mini White", 88, "White", false, "Single", 2700, 2700},
    {"LIFX Clean", 90, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Color", 91, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Color", 92, "Full Color", false, "Single", 1500, 9000},
    {"LIFX A19 US", 93, "Full Color", false, "Single", 1500, 9000},
    {"LIFX BR30", 94, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Candle White to Warm", 96, "Warm to White", false, "Single", 2200, 6500},
    {"LIFX A19", 97, "Full Color", false, "Single", 1500, 9000},
    {"LIFX BR30", 98, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Clean", 99, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Filament Clear", 100, "White", false, "Single", 2100, 2100},
    {"LIFX Filament Amber", 101, "White", false, "Single", 2000, 2000},
    {"LIFX A19 Night Vision", 109, "Full Color", true, "Single", 1500, 9000},
    {"LIFX BR30 Night Vision", 110, "Full Color", true, "Single", 1500, 9000},
    {"LIFX A19 Night Vision", 111, "Full Color", true, "Single", 1500, 9000},
    {"LIFX BR30 Night Vision", 112, "Full Color", true, "Single", 1500, 9000},
    {"LIFX Mini WW", 113, "Warm to White", false, "Single", 1500, 9000},
    {"LIFX Mini WW", 114, "Warm to White", false, "Single", 1500, 9000},
    {"LIFX Z", 117, "Full Color", false, "Linear", 1500, 9000},
    {"LIFX Z", 118, "Full Color", false, "Linear", 1500, 9000},
    {"LIFX Beam", 119, "Full Color", false, "Linear", 1500, 9000},
    {"LIFX Beam", 120, "Full Color", false, "Linear", 1500, 9000},
    {"LIFX Downlight", 121, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Downlight", 122, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Color", 123, "Full Color", false, "Single", 1500, 9000},
    {"LIFX Color", 124, "Full Color", false, "Single", 1500, 9000},
    {"LIFX White to Warm", 125, "Warm to White", false, "Single", 1500, 9000},
    {"LIFX White to Warm", 126, "Warm to White", false, "Single",  1500, 9000},
    {"LIFX White", 127, "Warm to White", false, "Single", 0, 0},
    {"LIFX White", 128, "Warm to White", false, "Single", 0, 0},
    {"LIFX Color", 129, "Full Color", false, "Single", 0, 0},
    {"LIFX Color", 130, "Full Color", false, "Single", 0, 0},
    {"LIFX White to Warm", 131, "Warm to White", false, "Single", 0, 0},
    {"LIFX White to Warm", 132, "Warm to White", false, "Single", 0, 0},
    {"LIFX White", 133, "Warm to White", false, "Single", 0, 0},
    {"LIFX White", 134, "Warm to White", false, "Single", 0, 0},
    {"LIFX GU10 Color", 135, "Full Color", false, "Single", 0, 0},
    {"LIFX GU10 Color", 136, "Full Color", false, "Single", 0, 0},
    {"LIFX Candle Color", 137, "Full Color", false, "Matrix", 0, 0},
    {"LIFX Candle Color", 138, "Full Color", false, "Matrix", 0, 0},
    {"LIFX Neon", 141, "Full Color", false, "Linear", 0, 0},
    {"LIFX Neon", 142, "Full Color", false, "Linear", 0, 0},
    {"LIFX String", 143, "Full Color", false, "Linear", 0, 0},
    {"LIFX String", 144, "Full Color", false, "Linear", 0, 0} 
    
};





// header should be filled out will (protocol, source, sequence defualt size)
// lxDev shoud have address and port

// set address and target of lxDev defore returning  
template<typename T>
Response* sendPacket(lx_frame_t* header, T* payload, lxDevice* lxDev, std::initializer_list<int> responseTypes) {


        // create socket 
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1) {
            perror("failed to create socket");
            exit(EXIT_FAILURE);
        }


        // bind set up socket to bind
        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = lxDev->port;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);




        // bind socket to recieve response
        if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            perror("Bind failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }


        // Set up the sending address structure
        struct sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = lxDev->port;
        clientAddr.sin_addr.s_addr = lxDev->address.s_addr;


        // combine header and payload 
        if (payload != nullptr) {
            header->size += sizeof(*payload);
        }

        std::vector<unsigned char> packet(header->size);
        std::memcpy(packet.data(), header, sizeof(*header));
        if (payload != nullptr) {
            std::memcpy(packet.data() + sizeof(*header), payload, sizeof(*payload));
        }


        // send packet
        ssize_t sent_bytes = sendto(sockfd, packet.data(), header->size, 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
        if (sent_bytes < 0) {
            perror("sendto failed");
            close(sockfd);
            return nullptr;
        }             


        // set up socket structure for receiving
        char buffer[BUFFER_SIZE];
        struct sockaddr_in senderAddr;
        socklen_t senderAddrLen = sizeof(senderAddr);
        

        // Set socket to non-blocking so we can stop listening after some time
        struct timeval tv;
        tv.tv_sec = 2;  // Wait for 2 seconds
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


        Response* resp = new Response();
        while(true) {
            
            ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                          (struct sockaddr*)&senderAddr, &senderAddrLen);

            if (recv_bytes < 0) {
                // Timeout or no more data, break the loop
                break;
            }

            if (recv_bytes < 36) { // minimum header size with no response payload      
                std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                continue;
            }


            lx_frame_t* response = (lx_frame_t*)buffer;
            //std::cout << "Response type: " << response->pkt_type << std::endl; ////////////////
            if (response->sequence == header->sequence && (std::find(responseTypes.begin(), responseTypes.end(), response->pkt_type) != responseTypes.end())) {

                    lxDev->address = senderAddr.sin_addr;
                    lxDev->target = response->target;
                    resp->size = recv_bytes - HEADER_SIZE;
                    resp->content = new char[resp->size];
                    resp->message_type = response->pkt_type;
                    memcpy((void*)resp->content, buffer + HEADER_SIZE, resp->size); 
                    close(sockfd);
                    return resp;
                
            } 
        }


    delete(resp->content);
    delete(resp);
    return nullptr;

}











// send braodcast and return ip of all lifx devices on the network
std::vector<in_addr> getAllDevices() {

        // create socket 
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1) {
            perror("failed to create socket");
            exit(EXIT_FAILURE);
        }

        // enable broadcast on socket
        int broadcastEnable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
            perror("Failed to set SO_BROADCAST");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // set upsocket to bind
        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(DEFAULT_LIFX_PORT);
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        // bind socket to recieve response
        if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            perror("Bind failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Set up the broadcast address structure
        struct sockaddr_in broadcastAddr;
        memset(&broadcastAddr, 0, sizeof(broadcastAddr));
        broadcastAddr.sin_family = AF_INET;
        broadcastAddr.sin_port = htons(DEFAULT_LIFX_PORT);
        broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);


        // set header data
        lx_frame_t curr;
        ssize_t sizeCurr = sizeof(curr);
        memset(&curr, 0, sizeCurr);
        curr.size = sizeCurr;
        curr.protocol = 1024;
        curr.addressable = 1;
        curr.tagged = 1;
        curr.source = SOURCE;
        curr.pkt_type = GET_SERVICE;


        // send boradcast getService message
        ssize_t sent_bytes = sendto(sockfd, &curr, sizeof(curr), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        if (sent_bytes < 0) {
            perror("sendto failed");
            close(sockfd);
            return {};
        } 

        // set up socket structure for receiving
        char buffer[BUFFER_SIZE];
        struct sockaddr_in senderAddr;
        socklen_t senderAddrLen = sizeof(senderAddr);


        // Set socket to non-blocking so we can stop listening after some time
        struct timeval tv;
        tv.tv_sec = 1;  // Wait for 2 seconds
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::vector<in_addr> devAddrs;
        while(true) {

            ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&senderAddr, &senderAddrLen);
            if (recv_bytes < 0) {
                // Timeout or no more data, break the loop
                break;
            }

            if (recv_bytes < 36) { // minimum header size with no response payload      
                std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                continue;
            }

            bool included = false;
            lx_frame_t* response = (lx_frame_t*)buffer;
            if (response->target != 0 && response->source == SOURCE && (response->reserved_1 != 0 || response->reserved_2 != 0 || response->reserved_3 != 0 || response->reserved_4 != 0 || response->reserved_5 != 0)) {

                if (devAddrs.size() == 0) {
                
                    devAddrs.push_back(senderAddr.sin_addr);
                    continue;
                } 
                for (int i = 0; i < devAddrs.size(); i++) {
                    if (senderAddr.sin_addr.s_addr == devAddrs[i].s_addr) {
                        included = true;
                    }
                }
                if (!included) {
                    devAddrs.push_back(senderAddr.sin_addr);
                }
            }            
        }

    close(sockfd);
    if (devAddrs.size() == 0) {
        
        return {};
    }
    return devAddrs;
}






bool listDevices() {

    std::vector<in_addr> addrs = getAllDevices();
    if (addrs.size() == 0) {
        std::cout << "No lifx devices found on your network" << std::endl;
        return false;
    }
    
    lx_frame_t header = {};
    lxDevice device = {};
    for (int i = 0; i < addrs.size(); i++) {

        // create header to send getService packet
        header.protocol = 1024;
        header.addressable = 1;
        header.pkt_type = GET_SERVICE;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        device.address = addrs[i];
        device.port = htons(DEFAULT_LIFX_PORT);


        // send and recieve getService packet
        Response* resp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp == nullptr) {
            std::cout << "No response from device to getService" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp->content));
        delete(resp->content);
        delete(resp);
        device.port = htons(sS->port);
        header.sequence++;


        // prepare, send, and recive getVersion packet
        header.pkt_type = GET_VERSION;
        Response* resp2 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_VERSION});                                                        
        if (resp2 == nullptr) {
            std::cout << "No response from device to getVersion" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        stateVersion* sV = const_cast<stateVersion*>(reinterpret_cast<const stateVersion*>(resp2->content));
        delete(resp2->content);
        delete(resp2);
        header.sequence++;
    

        // prepare, send, and recive getPower packet
        header.pkt_type = GET_POWER;
        Response* resp3 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_POWER_DEVICE});                           

        if (resp3 == nullptr) {
            std::cout << "No response from device to getPower" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        statePower* sP = const_cast<statePower*>(reinterpret_cast<const statePower*>(resp3->content));
        delete(resp3->content);
        delete(resp3);


        // prepare to print formatted output
        int deviceTypeWidth = 26;
        int ipAddressWidth = 14;
        int macAddressWidth = 21;
        int statusWidth = 6;

        const deviceInfo* devInfo = getDeviceInfo(sV->product); 
        std::string status = "Off";
        if (sP->level != 0) {
            status = "On";    
        }


        std::cout << "--------------------------------------------------------------------------------" << std::endl;
        std::cout << "| Device Type                | IP Address     | Mac Address           | Status | " << std::endl;
        std::cout << "--------------------------------------------------------------------------------" << std::endl;
        


        std::cout << "| " << std::setw(deviceTypeWidth) << std::left << devInfo->name << " | " << std::setw(ipAddressWidth) << std::left << inet_ntoa(device.address) << " | " << std::setw(macAddressWidth) << std::left << uint64ToMac(device.target) << " | " << std::setw(statusWidth) << std::left << status << " |" << std::endl;

        // zero out header and device
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));

    }
    return 0;

}



std::string uint64ToMac(uint64_t target) {

    std::ostringstream mac;
    for (int i = 5; i >= 0; --i) {
        mac << std::hex << std::setw(2) << std::setfill('0') << ((target >> (i * 8)) & 0xFF);
        if (i > 0) mac << ":";
    }
    return mac.str();

}



// return device info struct matching productId given by lifx light
const deviceInfo* getDeviceInfo(int productId) {
    int left = 0;
    int right = sizeof(lxDevs) / sizeof(lxDevs[0]) - 1; 

    while (left <= right) {
        int mid = left + (right - left) / 2;  

        if (lxDevs[mid].product == productId) {
            return &lxDevs[mid];  
        }
        if (lxDevs[mid].product < productId) {
            left = mid + 1;  
        } else {
            right = mid - 1;  
        }
    }
    
    return nullptr;
}




// make more efficient, maybe hashmap
bool isInParams(int count, char** params, std::string test) {

    for (int i = 0; i < count; i++) {
        if(strcmp(params[i], test.c_str()) == 0) {
            return true;    
        }
    }
    return false;
}


char* getNextParam(int count, char** params, std::string test) {
    
    for (int i = 0; i < count; i++) {
        if(strcmp(params[i], test.c_str()) == 0) {
            if ((i++) < count) {
                return params[i++];
            }    
        }
    }
    return nullptr;
}




// level = 0 for off, 65535 for on and anything else is its brightness
bool setPower(bool broadcast, bool brightness, uint32_t ip, int level, int duration) {

    lxDevice device = {};
    lx_frame_t header = {};

    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
    } else {
        in_addr addr = {};
        addr.s_addr = ip;
        addrs.push_back(addr);
    }

    for (int i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        header.pkt_type = GET_SERVICE;
        device.address.s_addr = addrs[i].s_addr;
        device.port = htons(DEFAULT_LIFX_PORT);
       

        // send and recieve getService packet
        Response* resp0 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp0 == nullptr) {
            std::cout << "[-] No response from device to getService - (setPow/2)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp0->content));
        device.port = htons(sS->port);
        header.sequence++;


        if (!brightness && (level == 0 || level == 65535)) {

            setLightPower setPow = {};
            setPow.duration = duration;
            header.pkt_type = 117;
            if (level == 0) {
                setPow.level = 0;
            } else {
                setPow.level = 65535;
            }
           
            // prepare, send, and recieve setLightPower packet
            header.ack_required = 1; 
            device.address = addrs[i];          
            Response* resp = sendPacket<setLightPower>(&header, &setPow, &device, {STATE_POWER_LIGHT, ACKNOWLEDGEMENT});
            if (resp == nullptr) {
                std::cout << "[-] No response to setPower packet - setPow/4" << std::endl;
                return false;
            }
            delete(resp->content);
            delete(resp);
            delete(resp0->content);
            delete(resp0);
        } else if (brightness) {

            // prepare, send, and recive a Get packet (for HSBK)
            header.pkt_type = GET;
            Response* resp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
            if (resp == nullptr) {
                std::cout << "[-] No response from device to Get packet - (setPower)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
                return false;
            }
            state* s = const_cast<state*>(reinterpret_cast<const state*>(resp->content));



            // prepare, send and recieve setColor packet
            setColor setCol = {};
            setCol.duration = duration;
            setCol.color = s->color;
            setCol.color.brightness = level;
            setCol.reserved = 0;
            header.pkt_type = SET_COLOR; 
            header.ack_required = 1;

            Response* resp2 = sendPacket<setColor>(&header, &setCol, &device, {STATE, ACKNOWLEDGEMENT});
            if (resp2 == nullptr) {
                std::cout << "[-] No response to setColor packet - setPow/5" << std::endl;
                return false;
            }
            
            delete(resp2->content);
            delete(resp2);
            delete(resp->content);
            delete(resp);
            
        } else {
            std::cout << "[-] Invalid level argument - (setPower)/6" << std::endl;
            return false;
        }

        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }


    return true;
}



bool printInfo(bool broadcast, uint32_t ip) {

    lx_frame_t header = {}; 
    lxDevice device = {};

    std::vector<in_addr> addrs;
    in_addr addr;
    if (broadcast) {
        addrs = getAllDevices();
    } else {
        addr.s_addr = ip;
        addrs.push_back(addr);
    }

    for (int i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        device.address.s_addr = addrs[i].s_addr;
        header.pkt_type = GET_SERVICE;
        device.port = htons(DEFAULT_LIFX_PORT);

        // send and recieve getService packet
        Response* resp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp == nullptr) {
            std::cout << "[-] No response from device to getService - (printInfo)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp->content));
        device.port = htons(sS->port);
        header.sequence++;

        // send and recieve getVersion packet
        header.pkt_type = GET_VERSION;
        Response* resp2 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_VERSION});                                                        
        if (resp2 == nullptr) {
            std::cout << "[-] No response from device to getVersion - (printInfo)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        stateVersion* sV = const_cast<stateVersion*>(reinterpret_cast<const stateVersion*>(resp2->content));
        const deviceInfo* devInfo = getDeviceInfo(sV->product); 
        header.sequence++;

        // prepare, send, and recive get packet (for label)
        header.pkt_type = GET;
        Response* resp4 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp4 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (printInfo)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp4->content));



        std::cout << "--------------------------------------------" << std::endl;
        std::cout << "-------------   Print Info    --------------" << std::endl;
        std::cout << "--------------------------------------------\n" << std::endl;
        std::cout << "----- Product Details" << std::endl;
        std::cout << "  Device Name: " << devInfo->name << std::endl;
        std::cout << "  Device Version: " << sV->version << std::endl;
        std::cout << "  Device Vecndor ID: " << sV->vendor << std::endl;
        std::cout << "  Device Product ID: " << devInfo->product << std::endl;
        std::cout << "  Device Color: " << devInfo->color << std::endl;
        (devInfo->infrared) ? std::cout << "  Device Infrared: True\n" : std::cout << "  Device Infrared: False\n";
        std::cout << "  Device zones: " << devInfo->zones << std::endl;
        std::cout << "  Device Minimum Kelvin: " << devInfo->kelvin_min << std::endl;
        std::cout << "  Device Maximum Kelvin: " << devInfo->kelvin_max << std::endl;

        std::cout << "\n\n----- Current State" << std::endl;
        std::cout << "  Device Power: " << s->power;
        (s->power == 0) ? std::cout << " (Off)\n" : std::cout << " (On)\n";
        std::cout << "  Device Hue: " << s->color.hue << std::endl;
        std::cout << "  Device Saturation: " << s->color.saturation << std::endl;
        std::cout << "  Device Brightness: " << s->color.brightness << std::endl;
        std::cout << "  Device Kelvin: " << s->color.kelvin << std::endl;


        std::cout << "\n\n----- Device Details" << std::endl;
        std::cout << "  Device Label: " << s->label << std::endl;
        std::cout << "  Device Address: " << inet_ntoa(device.address) << std::endl;
        std::cout << "  Device Port: " << htons(device.port) << std::endl;
        
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
        delete(resp->content);
        delete(resp);
        delete(resp2->content);
        delete(resp2);
        delete(resp4->content);
        delete(resp4);

    }
    return true;

}



HSBK* toHS(char* HEXORRGB) {

    int R, G, B;
    std::string hexOrRGB = HEXORRGB;

    // (rr, gg, bb) or rr, gg, bb
    if (hexOrRGB[0] == '(' && hexOrRGB[hexOrRGB.length()] == ')') {

        // strip ()
        if (hexOrRGB[0] == '(') {
            hexOrRGB = hexOrRGB.substr(1);
        }
        if (hexOrRGB[hexOrRGB.length() - 1] == ')') {
            hexOrRGB = hexOrRGB.substr(0, (hexOrRGB.length() - 1));
        }   
 
        size_t index = hexOrRGB.find(',');
        if (index == std::string::npos) {
            std::cout << "[-] Invalid RGB input format, use (rrr, ggg, bbb) or rrr, ggg, bbb - (toHSV)" << std::endl;
            return nullptr;
        }
        R = std::stoi(hexOrRGB.substr(0, index));
        hexOrRGB = hexOrRGB.substr(index + 1);


        index = hexOrRGB.find(',');
        if (index == std::string::npos) {
            std::cout << "[-] Invalid RGB input format, use (rrr, ggg, bbb) or rrr, ggg, bbb - (toHSV)" << std::endl;
            return nullptr;
        }
        G = std::stoi(hexOrRGB.substr(0, index));
        B = std::stoi(hexOrRGB.substr(index + 1));
                  
    } else if (hexOrRGB[0] == '0' && hexOrRGB[1] == 'x') {

        if (hexOrRGB[0] == '0' && hexOrRGB[1] == 'x' && hexOrRGB.length() == 8) {
            hexOrRGB = hexOrRGB.substr(2);
        } else if (hexOrRGB.length() != 6) {
            std::cout << "[-] Invalid hex format, use 0xRRGGBB - (toHSV)" << std::endl;
            return nullptr;
        }

        try {
            
            R = std::stoi(hexOrRGB.substr(0, 2), nullptr, 16); // R
            G = std::stoi(hexOrRGB.substr(2, 2), nullptr, 16); // G
            B = std::stoi(hexOrRGB.substr(4, 2), nullptr, 16); // B
        } catch (const std::exception& e) {
            std::cout << "[-] Error converting hex to RGB: " << e.what() << " - (toHSV)" << std::endl;
            return nullptr;
        }


    } else {
        std::cout << "[-] Color input invalid - (toHSV)" << std::endl;
        return nullptr;
    }

    double r = R / 255.0;
    double g = G / 255.0;
    double b = B / 255.0;

    double max = std::max(r, g);
    max = std::max(max, b);
    double min = std::min(r, g);
    min = std::min(min ,b);
    double delta = max - min;

    double S;
    (max == 0) ? S = 0 : S = ((delta)/max); 
    
    double H;
    if (max == 0) H = 0;
    else if (max == r) H = 60 * fmod((g - b) / delta, 6);
    else if (max == g) H = 60 * (((b - r) / delta) + 2);
    else if (max == b) H = 60 * (((r - g) / delta) + 4);
    if (H < 0) H += 360.0;

    uint16_t s = S * 65535;
    uint16_t h = (H / 360.0) * 65535;

    HSBK* hsbk = new HSBK();
    hsbk->hue = h;
    hsbk->saturation = s;
    hsbk->kelvin = 0;
    hsbk->brightness = 0;
        

    return hsbk;
}




bool setColorF(bool broadcast, uint32_t ip, int duration, char* color) {

    lx_frame_t header = {};
    lxDevice device = {};
    setColor setCol = {};
    HSBK* hsbk = toHS(color);
    if (hsbk == nullptr) {
        std::cout << "toHS failed - (setColor)" << std::endl;
        return false;
    }
 
    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
    } else {
        //in_addr* addr = new in_addr();
        in_addr addr = {};
        addr.s_addr = ip;
        addrs.push_back(addr);
    }

    for (int i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
 
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);

        // send and recieve getService packet
        Response* resp0 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp0 == nullptr) {
            std::cout << "[-] No response from device to getService - (setColor)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp0->content));
        device.port = htons(sS->port);
        header.sequence++;


        // prepare, send, and recive get packet (for label)
        header.pkt_type = GET;
        Response* resp1 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp1 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (setcolor)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp1->content));
        header.sequence++;


        // prepare, send, and recieve setColor
        header.pkt_type = SET_COLOR;
        hsbk->kelvin = s->color.kelvin;
        hsbk->brightness = s->color.brightness;
        setCol.color.hue = hsbk->hue;        
        setCol.color.saturation = hsbk->saturation;        
        setCol.color.brightness = hsbk->brightness;        
        setCol.color.kelvin = hsbk->kelvin;        
        setCol.duration = duration;

        header.ack_required = 1;
        Response* resp2 = sendPacket<setColor>(&header, &setCol, &device, {STATE, ACKNOWLEDGEMENT});
        if (resp2 == nullptr) {
            std::cout << "[-]/2 No response from device to setColor packet - (setcolor)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        state* s2 = const_cast<state*>(reinterpret_cast<const state*>(resp2->content));
        delete(resp2->content);
        delete(resp2);
        header.sequence++;
    
        delete(resp0->content);
        delete(resp0);
        delete(resp1->content);
        delete(resp1);
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }
    delete(hsbk);
    return true; 
}






void printHelp() {

    std::cout << "\n\nUsage Map: [command flag] {optinal argument} [select flag] {optinal argument} [optinal flags] {optinal arguments}" << std::endl;
    std::cout << "Order of flags does not matter, but arument for a flag must proceed it" << std::endl;
    std::cout << "ex. lifx -color 0xffffff -ip 192.168.8.141 -duration 1000" << std::endl;
    std::cout << "\n" << std::endl;
      
    std::cout << "---- Command flags" << std::endl;
    std::cout << "   -on" << std::endl;
    std::cout << "   -off" << std::endl;
    std::cout << "   -color {rgb or hex}                    ex. -color 0xffffff   or -color (rrr,ggg,bbb)" << std::endl;
    std::cout << "   -list                                  lists all devices on netowrk" << std::endl;
    std::cout << "   -info                                  provides verbose details about selected device(s)" << std::endl;
    std::cout << "   -brightness {percent}                  ex. -brightness 50" << std::endl;
    std::cout << "   -warmth {kelvin}                       kelvin: range 2500(warm) to  9000(cool) " << std::endl;
    std::cout << "   -multiColor {\"{list of hex or rgb}\"}       ex. -multiColor \"{0x46b34c, 0xf09f3d, 0x3debf0}\" " << std::endl;
    
    std::cout << std::endl;

    std::cout << "---- Select flags" << std::endl;
    std::cout << "   -ip {ipv4}                             ex. -ip 192.168.8.141" << std::endl;
    std::cout << "   -all                                   apply command to all devices on your network" << std::endl;
    std::cout << std::endl;

    std::cout << "---- Optinal flags" << std::endl;
    std::cout << "   -duration {time in ms}                 ex. -duration 100" << std::endl;
    std::cout << "\n" << std::endl;


    std::cout << "Typial Usage -----" << std::endl;
    std::cout << "> lifx -list                            (find device you want's ip)" << std::endl;
    std::cout << "> lifx -on -ip 192.168.8.141" << std::endl;
    std::cout << "> lifx -color 0x0000ff -ip 192.168.8.141" << std::endl;
    std::cout << "\n\n\n" << std::endl;
}





// Kelvin: range 2500° (warm) to 9000° (cool)
// set staturation to 0 to get rid of color
bool setKelvin(bool broadcast, uint32_t ip, int duration, uint16_t kelvin) {
    lx_frame_t header = {};
    lxDevice device = {};
    setColor setCol = {};
 
    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
    } else {
        in_addr addr = {};
        addr.s_addr = ip;
        addrs.push_back(addr);
    }

    for (int i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);

        // send and recieve getService packet
        Response* resp0 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp0 == nullptr) {
            std::cout << "[-] No response from device to getService - (setKelvin)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp0->content));
        device.port = htons(sS->port);
        header.sequence++;
    


        // prepare, send, and recive get packet (for label)
        header.pkt_type = GET;
        Response* resp4 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp4 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (setKelvin)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp4->content));
        header.sequence++;



        // prespare, send, and recive SetColor packet
        header.pkt_type = SET_COLOR;
        setCol.color.saturation = 0;
        setCol.color.hue = s->color.hue;
        setCol.color.brightness = s->color.brightness;
        setCol.color.kelvin = kelvin;
        setCol.duration = duration;

        
        header.ack_required = 1;
        Response* resp3 = sendPacket<setColor>(&header, &setCol, &device, {STATE, ACKNOWLEDGEMENT});
        if (resp3 == nullptr) {
            std::cout << "[-]/2 No response from device to setColor packet - (setKelvin)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        state* s2 = const_cast<state*>(reinterpret_cast<const state*>(resp3->content));
        header.sequence++;
   
        delete(resp0->content);
        delete(resp0);
        delete(resp4->content);
        delete(resp4);
        delete(resp3->content);
        delete(resp3);


 
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }
    return true;  
}




bool convertToUint16(const char* str, uint16_t& result) {
    try {
        // Convert char* to int using std::stoi
        int value = std::stoi(str);

        // Check if the value is within the range of uint16_t
        if (value < 0 || value > std::numeric_limits<uint16_t>::max()) {
            throw std::range_error("Value out of range for uint16_t");
        }

        // Return the value as uint16_t
        result =  static_cast<uint16_t>(value);
        return true;
    } catch (const std::invalid_argument& e) {
        // If std::stoi can't convert, handle the invalid input
        throw std::range_error("Invalid input, unable to convert to uint16_t");
    } catch (const std::out_of_range& e) {
        // If the number is too large for an int
        throw std::range_error("Value out of range for int");
    }
    return false;
}




bool charToUint32(const char* str, uint32_t& result) {
    // Check if input string is null
    if (str == nullptr) {
        std::cerr << "Error: Input string is null." << std::endl;
        return false;
    }

    // Initialize errno to 0 before conversion
    errno = 0;
    char* endPtr = nullptr;

    // Convert string to unsigned long using strtoul
    unsigned long temp = std::strtoul(str, &endPtr, 10);

    // Check for conversion errors
    if (errno == ERANGE || temp > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Error: Value out of range for uint32_t." << std::endl;
        return false;
    }

    // Check if the whole string was converted
    if (*endPtr != '\0') {
        std::cerr << "Error: Invalid characters found in input string." << std::endl;
        return false;
    }

    // Assign the result
    result = static_cast<uint32_t>(temp);
    return true;
}





// didnt work first time, then did after adding prints
// takes an array of hex values and randomy sets zones to one of the colors.
bool setColorZonesF(bool braodcast, uint32_t ip, uint32_t duration, std::vector<char*> colorVec) {
    
    lx_frame_t header = {};
    lxDevice device = {};
    std::vector<in_addr> addrs;
    if (braodcast) {
        addrs = getAllDevices();
    } else {
        in_addr addr = {};
        addr.s_addr = ip;
        addrs.push_back(addr); 
    }





    for (int i = 0; i < addrs.size(); i++) {
 
        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
 
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);


        // send and recieve getService packet
        Response* resp0 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_SERVICE});                                                     
        if (resp0 == nullptr) {
            std::cout << "[-] No response from device to getService - (setColorZonesF)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }      
        stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(resp0->content));
        device.port = htons(sS->port);
        header.sequence++;


        // prepare, send, and recive getVersion packet
        header.pkt_type = GET_VERSION;
        Response* resp2 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_VERSION});                                                 
        if (resp2 == nullptr) {
            std::cout << "No response from device to getVersion - (setColorZonesF)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
            return false;
        }
        stateVersion* sV = const_cast<stateVersion*>(reinterpret_cast<const stateVersion*>(resp2->content));
        header.sequence++;
       

 
        const deviceInfo* devInfo = getDeviceInfo(sV->product); 
        if (strcmp(devInfo->zones, "Extended Linear") == 0) {
        
            // prepare, send, and recive getExtendedColorZOnes packet
            getExtendedColorZones* getEColZon = new getExtendedColorZones(); 
            uint16_t brightness = 0;
            header.pkt_type = GET_EXTENDED_COLOR_ZONES;
            Response* resp3 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_EXTENDED_COLOR_ZONES});                                              
            if (resp3 == nullptr) {
                std::cout << "No response from device to getExtendedColorZones - (setColorZonesF)" << std::endl;                                                                // shoudl probably try a couple more times before giving up
                return false;
            } 
            stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(resp3->content));
            header.sequence++;
            brightness = sECZ->colors[0].brightness;
            int numZones = sECZ->zones_count;


            // convert from hex or rgb to hsbk
            std::vector<HSBK*> hsbVec;
            for (auto hex : colorVec) {
                HSBK* hsb = toHS(hex);
                hsbVec.push_back(hsb);
            }   
            
            // break up total zones into segments
            int colorSize = 82;

            int numSegments = 2*hsbVec.size(); 
            if (numSegments > numZones) numSegments = numZones;
        
            int segmentSize= numZones / numSegments;    // arbitrarily breaking zones into segments
            uint8_t remainder = static_cast<uint8_t>(numZones % numSegments);
            int numMessages = ceil((double)numZones / colorSize);
            


                // generate random number
                std::random_device rd; 
                std::mt19937 gen(rd()); 
                std::uniform_int_distribution<> distr(0, (hsbVec.size() - 1)); 
                int randIndex = distr(gen);



                // loops though all zones, broken up into multiple mesages if more than 82
                setExtendedColorZones setEColZon = {};
                int numZonesLeft = numZones;
                setEColZon.color_count = numZones;
                for (int v = 0; v < numMessages; v++) {

                    setEColZon.duration = duration;
                    setEColZon.zone_index = v * colorSize;

                    for (int x = 0; x < numZonesLeft; x++) {
                        

                        if (x % segmentSize == 0) { // next color
                            randIndex = distr(gen);
                        }

                        setEColZon.colors[x + (v * colorSize)].hue = hsbVec[randIndex]->hue;
                        setEColZon.colors[x + (v * colorSize)].saturation = hsbVec[randIndex]->saturation;
                        setEColZon.colors[x + (v * colorSize)].brightness = brightness;
                        setEColZon.colors[x + (v * colorSize)].kelvin = hsbVec[randIndex]->kelvin;
                        

                        // if last zone, or reached max zones for 1 message
                        if (x == (numZonesLeft - 1) || x == (colorSize - 1)) {
                            ((v + 1) == numMessages) ? setEColZon.apply = APPLY : setEColZon.apply = NO_APPLY;

                            // prepare, send, and recive getVersion packet
                            header.ack_required = 1;
                            header.pkt_type = SET_EXTENDED_COLOR_ZONES;
                            Response* resp4 = sendPacket<setExtendedColorZones>(&header, &setEColZon, &device, {STATE_EXTENDED_COLOR_ZONES, ACKNOWLEDGEMENT});                                                        
                            if (resp4 == nullptr ) {
                                std::cout << "No response from device to setExtendedColorZones - (setColorZonesF)" << std::endl;                                                                // shoudl probably try a couple more times before giving up
                                return false;
                            } 
                            //stateExtendedColorZones* sECZ2 = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(resp4->content));

                            delete(resp4->content);
                            delete(resp4);
                            break;
                        } 

                    }

                    memset(&setEColZon, 0, sizeof(setEColZon));
                    numZonesLeft = numZones - colorSize;
                }
            
        for (HSBK* hsb: hsbVec) {
            delete(hsb);
        }

        delete(resp0->content);
        delete(resp0);
        delete(resp2->content);
        delete(resp2);
        delete(resp3->content);
        delete(resp3);
        } else if (strcmp(devInfo->zones, "Linear") == 0) {
    
            int numZones = 0;


            // get how many color zones there are
            getColorZones gColZones = {};
            gColZones.start_index = 0;
            gColZones.end_index = 255;




            // prepare, send, and recive getColorZones packet
            header.pkt_type = GET_COLOR_ZONES;
            Response* resp3 = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE});                                                         
            uint16_t brightness = 0; 
            if (resp3 == nullptr) {
                std::cout << "No response from device to getColorZones - (setColorZonesF)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
                return false;
            }

            // get ZoneCount and brightness to use alter
            if (resp3->message_type = STATE_ZONE) {  // only 1 zone
                stateZone* sZ = const_cast<stateZone*>(reinterpret_cast<const stateZone*>(resp3->content));
                numZones = sZ->zone_count;
                brightness = sZ->brightness;


            } else if (resp3->message_type == STATE_MULTI_ZONE) {
                stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(resp3->content));
                numZones = sMZ->zones_count;
                brightness = sMZ->colors[0].brightness;


            } else {
                std::cout << "[-] unexpected response from getColorZones - (setColorZonesF) " << std::endl;
                return false;
            }
            header.sequence++;
            

    
            // convert from hex or rgb to hsbk
            std::vector<HSBK*> hsbVec;
            for (auto hex : colorVec) {
                HSBK* hsb = toHS(hex);
                hsbVec.push_back(hsb);
            }   



            // break up zones

            int numSegments = 2*hsbVec.size(); 
            if (numSegments > numZones) numSegments = numZones;
            int segmentSize= numZones / numSegments;    // arbitrarily breaking zones into segments
            int remainder = numZones % numSegments;

            



            setColorZones setColZon = {};
            memset(&setColZon, 0, sizeof(setColZon));
            for (int x = 0; x <= numSegments; x++) {
            
                
                // generate random number
                std::random_device rd; 
                std::mt19937 gen(rd()); 
                std::uniform_int_distribution<> distr(0, (hsbVec.size() - 1)); 
                int randIndex = distr(gen);
                
                setColZon.start_index = static_cast<uint8_t>(x * segmentSize);
                setColZon.hue = hsbVec[randIndex]->hue;
                setColZon.saturation = hsbVec[randIndex]->saturation;
                setColZon.brightness = brightness;
                setColZon.kelvin = hsbVec[randIndex]->kelvin;
                setColZon.duration = duration;



                // check if at end to add remainder and change apply
                if (x == numSegments) {
                    setColZon.end_index = static_cast<uint8_t>((x * segmentSize) + remainder);
                    setColZon.apply = APPLY;  
                    
                } else {
                    setColZon.end_index = static_cast<uint8_t>((x * segmentSize) + segmentSize);
                    setColZon.apply = NO_APPLY;          
                } 



                // prepare, send, and recive setColorZones packet
                header.ack_required = 1;
                header.pkt_type = SET_COLOR_ZONES;
                Response* resp4 = sendPacket<setColorZones>(&header, &setColZon, &device, {STATE_MULTI_ZONE, ACKNOWLEDGEMENT});      // setColorZone shoudl reply with stateMultiZone, but it responsed withe acknowledgement. No clue why                                                  
                if (resp4 == nullptr ) {
                    std::cout << "No response from device to setColorZones - (setColorZonesf)" << std::endl;                                                  // shoudl probably try a couple more times before giving up
                    return false;
                }
                stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(resp4->content));
                delete(resp4->content);
                delete(resp4);
                header.sequence++;
                memset(&setColZon, 0, sizeof(setColZon));
            }

        
        delete(resp3->content);
        delete(resp3);
        } else {
            std::cout << "[-] Device:" << devInfo->name << " does not support multiple color zones" << std::endl;
            return false;
        }
    }
    return 0;
}










std::vector<char*> parseColorList(char* list) {
    std::vector<char*> result;
    std::string str(list);
    // Ensure input starts and ends with braces
    if (str.front() != '{' || str.back() != '}') {
        std::cerr << "Improper format for list of colors. -help for usage details\n";
        return {};
    }

    // Remove the outer braces
    str = str.substr(1, str.size() - 2);

    // Parse the list
    while (!str.empty()) {
        // Remove leading spaces and commas
        str.erase(0, str.find_first_not_of(" ,"));

        // Hex value
        if (!str.empty() && str.front() == '0') {
            std::string hexValue = str.substr(0, 8); // Assume format 0xRRGGBB
            if (hexValue.length() != 8 || hexValue.substr(0, 2) != "0x") {
                std::cerr << "Invalid hex value format\n";
                return {};
            }
            char* hexChar = new char[hexValue.size() + 1];
            std::strcpy(hexChar, hexValue.c_str());
            result.push_back(hexChar);
            str = str.substr(8); // Skip the parsed hex value
        }
        // RGB value
        else if (!str.empty() && str.front() == '(') {
            size_t closeIndex = str.find(')');
            if (closeIndex == std::string::npos) {
                std::cerr << "Invalid RGB value format\n";
                return {};
            }
            std::string rgbValue = str.substr(0, closeIndex + 1);
            char* rgbChar = new char[rgbValue.size() + 1];
            std::strcpy(rgbChar, rgbValue.c_str());
            result.push_back(rgbChar);
            str = str.substr(closeIndex + 1); // Skip the parsed RGB value
        }
        // Invalid format
        else if (!str.empty()) {
            std::cerr << "Improper format for list of colors. -help for usage details\n";
            return {};
        }
    }

    return result;
}


void genSource() {
    
        // generate random number 1 - 1000000
        std::random_device rd; 
        std::mt19937 gen(rd()); 
        std::uniform_int_distribution<> distr(0, (1e6 - 1)); 
        SOURCE = distr(gen);
}





int main(int argc, char* argv[]) {

    uint32_t ip;
    bool broadcast = false;
    uint32_t duration = 0; 
    genSource();

    // help
    if (isInParams(argc, argv, "-help") || isInParams(argc, argv, "--help") || isInParams(argc, argv, "-h")) {
        printHelp();
        return 0;
    }

    if (isInParams(argc, argv, "-list")) {
        bool listDevsResult = listDevices();
        if (!listDevsResult)
            return -1;
        return 0;
    }



    // duration
    if (isInParams(argc, argv, "-duration")) {
        char* durStr = getNextParam(argc, argv, "-duration");
        if (durStr == nullptr) {
            std::cout << "[-] Invalid duration value, use -help for instructions - (m)" << std::endl;
            return -1;
        }
        bool durationResult = charToUint32(durStr, duration); 
        if (!durationResult) {
            std::cout << "[-] Invalid duration value: " << durStr << ",  use -help for proper usage - (m)" << std::endl;
            return -1;
        }
    }



    // ip
    if (isInParams(argc, argv, "-ip")) {
        char* ipPtr = getNextParam(argc, argv, "-ip");
        if (ipPtr == nullptr) {
            std::cout << "[-] invalid ip - EX. -ip 192.168.1.22" << std::endl;
            return -1;
        }
        in_addr deviceIp;
        int ipResult = inet_aton(ipPtr, &deviceIp);
        if (!ipResult) {
            std::cout << "[-] IP address invalid: " << ipPtr << ", use -help for proper usage - (m)" << std::endl;
            return -1;
        }
        ip = deviceIp.s_addr;
    } else if (isInParams(argc, argv, "-all")) {
        ip = 0;
        broadcast = true;
    } else {
        std::cout << "[-] device(s) selected, use -help for proper usage - (m)" << std::endl;
        return -1;
    }




    // command flags 
    if (isInParams(argc, argv, "-on")) {
        bool onResult = setPower(broadcast, false, ip, 65535, duration);
        if (!onResult)             
            return -1;
        return 0;

    } else if (isInParams(argc, argv, "-off")) {
        bool offResult = setPower(broadcast, false, ip, 0, duration);
        if (!offResult)             
            return -1;
        return 0;

    } else if (isInParams(argc, argv, "-color")) {
        char* color = getNextParam(argc, argv, "-color");
        if (color == nullptr) {
            std::cout << "[-] invalid color provided, use -help for proper usage" << std::endl;
            return -1;
        }
        setColorF(broadcast, ip, duration, color);  
        return 0;        

    } else if (isInParams(argc, argv, "-warmth")) {

        
        // get, check, and convert brightness value from params
        uint16_t kelvin = 0;  
        char* wStr = getNextParam(argc, argv, "-warmth");
        if (wStr == nullptr) {
            std::cout << "[-] Invalid warmth value, use -help for instructions - (m)" << std::endl;
        }            
        bool warmthResult;
         try {
            warmthResult = convertToUint16(wStr, kelvin);
        } catch (const std::range_error& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            std::cout << "[-] Invalid warmth value: " << wStr << "     - (main/warmth)" << std::endl;
            return -1;
        }
        if (kelvin < 2500 || kelvin > 9000) {
            std::cout << "Warmth value " << kelvin << " out of range, must be between 2500 to 9000" << std::endl;
            return -1;
        }

        warmthResult = setKelvin(broadcast, ip, duration, kelvin);
        if (!warmthResult) 
            return -1;
        return 0;


    }  else if (isInParams(argc, argv, "-info")) {
        bool infoResult = printInfo(broadcast, ip); 
        if (!infoResult)
            return -1;
        return 0;
    
    } else if (isInParams(argc, argv, "-brightness")) {

        
        // get, check, and convert brightness value from params
        char* bStr = getNextParam(argc, argv, "-brightness");
        if (bStr == nullptr) {
            std::cout << "[-] invalid brightness value, use -help for proper usage - (m)" << std::endl;
            return -1;
        }
        uint16_t tempBrightness;
        bool brightnessResult = convertToUint16(bStr, tempBrightness);
        if (!brightnessResult)
            return -1;
        if (tempBrightness > 100) {
            std::cout << "[-] invalid brightness, value entered " << tempBrightness << " shoud be percent so cannot exceed 100" << std::endl;
            return -1;
        }
        float tempBrightness2 = static_cast<float>(tempBrightness) / 100.0f;
        uint16_t brightness = static_cast<uint16_t>(65535 * tempBrightness2);
    

        bool setPowerResult = setPower(broadcast, true, ip, brightness, duration);
        if (!setPowerResult)             
            return -1;
        return 0;

    }  else if (isInParams(argc, argv, "-multiColor")) {


        char* list =  getNextParam(argc, argv, "-multiColor");
        std::vector<char*> hexOrRGBVec = parseColorList(list);
        if (hexOrRGBVec.empty()) {
            return -1;
        }



        setColorZonesF(false, ip, 0, hexOrRGBVec);
    // bool setColorZonesF(bool braodcast, uint32_t ip, uint32_t duration, char** coloArr) {








   } else {
        std::cout << "[-] no comand flag provided, use -help for proper usage" << std::endl;
        return -1;
    }

    return 0;
}



















