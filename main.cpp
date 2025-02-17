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
#include <optional>
#include <fstream>
#include <getopt.h>

#include "structsAndDefs.h"
#include "json.hpp"




uint32_t SOURCE;


std::vector<in_addr> getAllDevices();
bool listDevices();
std::string uint64ToMac(uint64_t target);
bool setPower(bool broadcast, bool brightness, uint32_t ip, uint16_t level, uint32_t duration);
std::vector<HSBK*> getMultiZoneData(lx_frame_t* header, lxDevice* device, bool extended_multizone=false);
bool setMultiZoneLight(lx_frame_t* header, lxDevice* device, uint32_t duration, std::vector<HSBK*> hsbkVec, bool extended_multizone=false);
extendedDeviceInfo* getExtendedDeviceInfo(lx_frame_t* header, lxDevice* device);
bool saveScene(uint32_t ip, std::string path, std::string fileName, std::string sceneName);
bool loadScene(uint32_t ip, std::string pathToScene, std::string sceneName);
bool setKelvin(bool broadcast, uint32_t ip, uint32_t duration, uint16_t kelvin);
bool printInfo(bool broadcast, uint32_t ip);
bool setColorF(bool broadcast, uint32_t ip, uint32_t duration, HSBK* hsbk);
void genSource();
bool initScene(nlohmann::json& scene, const std::string& scenePath, int numZones);
uint16_t strToUint16(const char* str);
uint32_t charToUint32(const char* str);
bool setColorZonesF(uint32_t ip, uint32_t duration, bool random, std::vector<HSBK*>& hsbkVec);
std::vector<HSBK*>* strToHSBKVec(const char* colorInput);
void printHelp();
bool parseArgs(int argc, char** argv, inputArgs* options);







/// @brief sends lifx message useing udp socket
/// @tparam T is struct coresponding to content of message, use nullPtr struct if none
/// @param header struct conting information about lifx packet
/// @param optinal if message has a body
/// @param device struct containing ip and port of target device
/// @param responseTypes is initializer list of all acceptable lifx responseTypes
/// @return if response returns content in response struct, if not returns nullptr
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


        for (int attempts = 3; attempts > 0; attempts--) {

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
                
                ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&senderAddr, &senderAddrLen);

                if (recv_bytes < 0) {
                    // error
                    break;
                }

                if (recv_bytes < 36) { // minimum header size with no response payload      
                    std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                    continue;
                }


                lx_frame_t* response = (lx_frame_t*)buffer;
                std::cout << "Response type: " << response->pkt_type << std::endl; //////////////////////////////////
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
        }

    close(sockfd); 
    return nullptr;

}








/// @brief sends lifx message useing udp socket and recieves multiple resposes 
/// @tparam T is struct coresponding to content of message, use nullPtr struct if none
/// @param header struct conting information about lifx packet
/// @param optinal if message has a body
/// @param device struct containing ip and port of target device
/// @param responseTypes is initializer list of all acceptable lifx responseTypes
/// @return if responses returns vector of their contents, if not return empty vector
template<typename T>
std::vector<Response*> sendMultiPacket(lx_frame_t* header, T* payload, lxDevice* lxDev, std::initializer_list<int> responseTypes, int numResponses) {


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



        std::vector<Response*> responses;
        for(int attempts = 3; attempts > 0; attempts--) {

            // send packet
            ssize_t sent_bytes = sendto(sockfd, packet.data(), header->size, 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
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
            tv.tv_sec = 2;  // Wait for 2 seconds
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


            int respsSoFar = 0;
            while(respsSoFar < numResponses) {
                
                ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                              (struct sockaddr*)&senderAddr, &senderAddrLen);

                if (recv_bytes < 0) {
                    // Timeout or no more data, break the loop
                    break;
                }

                if (recv_bytes < 36) { // minimum header size with no response payload      
                    //std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                    continue;
                }


                lx_frame_t* response = (lx_frame_t*)buffer;
                //std::cout << "Response type: " << response->pkt_type << std::endl; ////////////////
                if (response->sequence == header->sequence && (std::find(responseTypes.begin(), responseTypes.end(), response->pkt_type) != responseTypes.end())) {
  
                        Response* resp = new Response();
                        lxDev->address = senderAddr.sin_addr;
                        lxDev->target = response->target;
                        resp->size = recv_bytes - HEADER_SIZE;
                        resp->content = new char[resp->size];
                        resp->message_type = response->pkt_type;
                        memcpy((void*)resp->content, buffer + HEADER_SIZE, resp->size); 
                        responses.push_back(resp);
                } 
            }


            if (respsSoFar == numResponses) {
                close(sockfd);
                return responses;
            } 
        }
   

        close(sockfd);
        for (Response* resp : responses) {
            delete resp;
        }
        std::cout << "[-] Invalid number of responses - sendPacket" << std::endl;
        return {};

}







/// @breif send broadcast message to all devices on network
/// @return return vector of in_addrs of unique devies, or empty vector if none
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







/// @breif print the type, ip, mac, and current state of every lifx device on network
/// @return bool success
bool listDevices() {


    std::vector<in_addr> addrs = getAllDevices();
    if (addrs.size() == 0) {
        std::cout << "No lifx devices found on your network" << std::endl;
        return false;
    }
    
    lx_frame_t header = {};
    lxDevice device = {};
    bool first = true;
    for (int i = 0; i < addrs.size(); i++) {

        //- setup header
        header.protocol = 1024;
        header.addressable = 1;
        header.pkt_type = GET_SERVICE;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        device.address = addrs[i];
        device.port = htons(DEFAULT_LIFX_PORT);
    


        extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
        if(extDevInfo == nullptr) {
            return false;
        }
        std::string productName = std::string(extDevInfo->name);



        //- send getPower, recive statePowerDevice
        header.pkt_type = GET_POWER;
        Response* resp3 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_POWER_DEVICE});                           

        if (resp3 == nullptr) {
            std::cout << "No response from device to getPower" << std::endl;
            return false;
        }
        statePower* sP = const_cast<statePower*>(reinterpret_cast<const statePower*>(resp3->content));



        //- prepare to print formatted output
        int deviceTypeWidth = 26;
        int ipAddressWidth = 14;
        int macAddressWidth = 21;
        int statusWidth = 6;

     
        std::string status = "Off";
        if (sP->level != 0) {
            status = "On";    
        }



        if (first) {
            std::cout << "--------------------------------------------------------------------------------" << std::endl;
            std::cout << "| Device Type                | IP Address     | Mac Address           | Status | " << std::endl;
            std::cout << "--------------------------------------------------------------------------------" << std::endl;
            first = false;
        } 



        std::cout << "* " << std::setw(deviceTypeWidth) << std::left << productName << " | " << std::setw(ipAddressWidth) << std::left << inet_ntoa(device.address) << " | " << std::setw(macAddressWidth) << std::left << uint64ToMac(device.target) << " | " << std::setw(statusWidth) << std::left << status << " |" << std::endl;



    
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
        delete(resp3->content);
        delete(resp3);
    }
   
    std::cout << "\n\n"; 
    return true;

}






/// @breif conver a uint64 to a human readable mac adress 
/// @return string human readable mac address
std::string uint64ToMac(uint64_t target) {

    std::ostringstream mac;
    for (int i = 5; i >= 0; --i) {
        mac << std::hex << std::setw(2) << std::setfill('0') << ((target >> (i * 8)) & 0xFF);
        if (i > 0) mac << ":";
    }
    return mac.str();

}











/// @breif turn light on/off or set brightness
/// @param brodcast bool if function should be applied to all devices
/// @param brightness bool if level arg should be treated as on/off or brightness level
/// @param ip uint32 ip adress, 0 if broadcast
/// @param level if !brightness 65535 turn on, if 0 turn off, else level is percent of brightness
/// @param duration how long should action take to finish
/// @return bool sucess
bool setPower(bool broadcast, bool brightness, uint32_t ip, uint16_t level, uint32_t duration) {

    lxDevice device = {};
    lx_frame_t header = {};

    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
        if (addrs.empty()) {
            std::cerr << "No lifx devices on the network" << std::endl;
            return false;
        }
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
       
        

        extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
        if(extDevInfo == nullptr) {
            return false;
        }


        //- if turn light on/off
        if (!brightness && (level == 0 || level == 65535)) {

            setLightPower setPow = {};
            setPow.duration = duration;
            header.pkt_type = 117;
            if (level == 0) {
                setPow.level = 0;
            } else {
                setPow.level = 65535;
            }
           

            //- send setLightPower, recieve statePowerLight
            header.ack_required = 1; 
            device.address = addrs[i];          
            Response* setPowResponse = sendPacket<setLightPower>(&header, &setPow, &device, {STATE_POWER_LIGHT, ACKNOWLEDGEMENT});
            if (setPowResponse == nullptr) {
                std::cout << "[-] No response to setPower packet - setPow/4" << std::endl;
                return false;
            }
            delete(setPowResponse->content);
            delete(setPowResponse);
            delete(extDevInfo);


        //- if change brightness
        } else if (brightness) {


            //- check if light has multi zone capabilites
            if (extDevInfo->multizone || extDevInfo->extended_multizone) {
            
                //- send getColorZones, reviece stateMultiZone or stateZone - use response to determine how mnay zone and if more responses are needed
                getColorZones gColZones = {};
                gColZones.start_index = 0;
                gColZones.end_index = 255;
                header.pkt_type = GET_COLOR_ZONES;
                Response* getColResponse = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE});                                 
                if (getColResponse == nullptr) {
                    std::cout << "No response from device to getColorZones - (setPower)" << std::endl;
                    return false;
                }
                stateZone* sZ = const_cast<stateZone*>(reinterpret_cast<const stateZone*>(getColResponse->content));
                header.sequence++;


                //- light currently more than 1 color
                if (getColResponse->message_type == STATE_MULTI_ZONE) {
                




                    //- get hsbk for all zones and change brightness 
                    std::vector<HSBK*> hsbkVec = getMultiZoneData(&header, &device, extDevInfo->extended_multizone);
                    for(HSBK* hsbk: hsbkVec) {
                        hsbk->brightness = level;
                    }


                    // call function to set the light and pass hsbkVec                    
                    bool setLightResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo->extended_multizone);
                



                for (HSBK* hsbk: hsbkVec) {
                    delete(hsbk);
                }
                delete(getColResponse->content);
                delete(getColResponse);
                delete(extDevInfo);
                return true;

                }
            }


 
            //- if light using single color 
            //- send get, recive state - for current
            header.pkt_type = GET;
            Response* getResp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
            if (getResp == nullptr) {
                std::cout << "[-] No response from device to Get packet - (setPower)" << std::endl;                     
                return false;
            }
            state* s = const_cast<state*>(reinterpret_cast<const state*>(getResp->content));



            // prepare, send and recieve setColor packet
            setColor setCol = {};
            setCol.duration = duration;
            setCol.color = s->color;
            setCol.color.brightness = level;
            setCol.reserved = 0;
            header.pkt_type = SET_COLOR; 
            header.ack_required = 1;
            Response* setColResponse = sendPacket<setColor>(&header, &setCol, &device, {STATE, ACKNOWLEDGEMENT});
            if (setColResponse == nullptr) {
                std::cout << "[-] No response to setColor packet - setPow/5" << std::endl;
                return false;
            }


            
            delete(getResp->content);
            delete(getResp);
            delete(setColResponse->content);
            delete(setColResponse);
            delete(extDevInfo);


        } else {
            std::cout << "[-] Invalid level argument - (setPower)/6" << std::endl;
            return false;
        }

        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }

    return true;
}








/// @breif gets the hsbk info for each zone
/// @param
/// @param
/// @param
/// @param
/// @param
/// @param
/// @return vector of all hsbk values, empty if error
//std::vector<HSBK*> getMultiZoneData(lx_frame_t* header, lxDevice* device, bool extended_multizone=false) {
std::vector<HSBK*> getMultiZoneData(lx_frame_t* header, lxDevice* device, bool extended_multizone) {


    if (!extended_multizone) {

        getColorZones gColZones = {};
        gColZones.start_index = 0;
        gColZones.end_index = 255;
        int numZones = 0;
        int numMesages = 0;

        
        //- send getColorZones, reviece stateMultiZone or stateZone - use response to determine how mnay zone and if more responses are needed
        header->pkt_type = GET_COLOR_ZONES;
        Response* resp3 = sendPacket<getColorZones>(header, &gColZones, device, {STATE_ZONE, STATE_MULTI_ZONE});                                                         
        if (resp3 == nullptr) {
            std::cout << "No response from device to getColorZones - (getMultiZoneData)" << std::endl;
            return {};
        }
        header->sequence++;

        //- light currently only 1 color
        if (resp3->message_type == STATE_ZONE) {
            std::cerr << "[-] device selected " <<  inet_ntoa(device->address) << " - only displaying 1 color, not scene" << std::endl;
            return {};


        //- multiple responses needed to cature all zones
        } else if (resp3->message_type == STATE_MULTI_ZONE) {
            stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(resp3->content));
            int numZones = sMZ->zones_count;
            
            //- calculate how many messages to expect
            int number_segments_of_8 = (gColZones.end_index - gColZones.start_index) / 8;
            int temp = floor(number_segments_of_8) + 1;
            int count_from_request = std::max(1, temp);        

            int count_from_response = ceil(sMZ->zones_count / 8);
            int expected_number_of_messages = std::min(count_from_request, count_from_response);


            //- send getColorZones, reviece stateMultiZone - returns multiple mesages each with an array of 8 hsbk
            std::vector<Response*> responses = sendMultiPacket<getColorZones>(header, &gColZones, device, {STATE_MULTI_ZONE}, expected_number_of_messages); 
            if (responses.empty()) {
                std::cout << "No response from device to getColorZones - (getMultiZoneData)" << std::endl;
                return {};
            }
            header->sequence++;


            
            //- use multiple array of 8 HSBK structs in multiple responses to insert into hsbk vector
            std::vector<HSBK*> hsbkVec;
            for (int i = 0; i < responses.size(); i++) {
                for (int x = ((stateMultiZone*)responses[i]->content)->zone_index; x < numZones; x++) {
                    hsbkVec.push_back(&((stateMultiZone*)responses[i]->content)->colors[x]);
                    HSBK* hsbk = new HSBK();
                    hsbk->hue = ((stateMultiZone*)responses[i]->content)->colors[x].hue;
                    hsbk->saturation = ((stateMultiZone*)responses[i]->content)->colors[x].saturation;
                    hsbk->brightness = ((stateMultiZone*)responses[i]->content)->colors[x].brightness;
                    hsbk->kelvin = ((stateMultiZone*)responses[i]->content)->colors[x].kelvin;
                }
            }


        delete(resp3->content);
        delete(resp3);
        for (auto resps: responses) {
            delete(resps);
        }


        } else {
            std::cout << "[-] unexpected response from getColorZones - (getMultiZoneData) " << std::endl;
            return {};
        }


    } else if (extended_multizone) {



        int sizeOfColorArray = 82;
        //- send getExtendedColorZones, recieve stateExtendedColorZones - use response to determine if more than 1 response needed
        header->pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* resp3 = sendPacket<nullPtr>(header, nullptr, device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (resp3 == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (getMultiZoneData)" << std::endl;
            return {};
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(resp3->content));
        header->sequence++;
        int numZones = sECZ->zones_count;
        int numMessages = ceil(sECZ->zones_count / sizeOfColorArray);
        std::vector<HSBK*> hsbkVec;

        if (numMessages <= 1) {

            //- insert the array of HSBK structs in sECZ->color into hsbk vector
            for (int i = 0; i < numZones; i++) {
                HSBK* hsbk = new HSBK();
                hsbk->hue = sECZ->colors[i].hue;
                hsbk->saturation = sECZ->colors[i].saturation;
                hsbk->kelvin = sECZ->colors[i].kelvin;
                hsbk->brightness = sECZ->colors[i].brightness;
                hsbkVec.push_back(hsbk);
            }
            return hsbkVec;


        } else {
            //- send getExtendedColorZones, recieve stateExtendedColorZones - returns 1 or more messages with hsbk arrays 
            std::vector<Response*> responses = sendMultiPacket<nullPtr>(header, nullptr, device, {STATE_EXTENDED_COLOR_ZONES}, numMessages);    
            if (responses.empty()) {
                std::cout << "No response from device to getExtendedColorZones - (getMultiZoneData)" << std::endl;   
                return {};
            } 
            
            //- use the array of HSBK structs from the multiple responses to insert into hsbk vector
            for (int x = 0; x < numMessages; x++) {
                for (int i = 0; i < numZones; i++) {
                    hsbkVec.push_back(&((stateExtendedColorZones*)responses[x]->content)->colors[i]);
                }
            }
            return hsbkVec;
        }

    delete(resp3->content);
    delete(resp3);


    } else {
        std::cerr << "[-] Unknown Error - (getMultiZoneData)" << std::endl;
    }
    return {};
}
















/// @breif set the hsbk values of the state of multi-zone light
///
///
///
/// @param hsbkVec vector of hsbk* with the same length as number of zones of target device
/// 
//bool setMultiZoneLight(lx_frame_t* header, lxDevice* device, uint32_t duration, std::vector<HSBK*> hsbkVec, bool extended_multizone=false) {
bool setMultiZoneLight(lx_frame_t* header, lxDevice* device, uint32_t duration, std::vector<HSBK*> hsbkVec, bool extended_multizone) {



    if (!extended_multizone) {
            int numZones = 0;


            //- send getColorZones - to get number of zones
            getColorZones gColZones = {};
            gColZones.start_index = 0;
            gColZones.end_index = 255;
            header->pkt_type = GET_COLOR_ZONES;
            Response* colZonResp = sendPacket<getColorZones>(header, &gColZones, device, {STATE_ZONE, STATE_MULTI_ZONE});                                     
            if (colZonResp == nullptr) {
                std::cout << "No response from device to getColorZones - (setMultiZonelight)" << std::endl;
                return false;
            }
            header->sequence++;
            if (colZonResp->message_type == STATE_ZONE) {  
                stateZone* sZ = const_cast<stateZone*>(reinterpret_cast<const stateZone*>(colZonResp->content));
                numZones = sZ->zone_count;
            } else if (colZonResp->message_type == STATE_MULTI_ZONE) {
                stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(colZonResp->content));
                numZones = sMZ->zones_count;
            } else {
                std::cout << "[-] unexpected response from getColorZones - (setMultiZonelight) " << std::endl;
                return false;
            }
            


            if (numZones != hsbkVec.size()) {
        
                std::cerr << "[-] number of zones of selected device does not match provided hsbk vector - (setMultiZonelight)" << std::endl;
                return false;
            } 
            

                       
            //- loop through all zones and set each to corresponding color from hsbk vector
            for (int i = 0; i < numZones; i++) {
                
                setColorZones* setColZon = {};
                setColZon->start_index = i;
                setColZon->end_index = i;
                setColZon->duration = duration;
                
                setColZon->hue = hsbkVec[i]->hue;
                setColZon->saturation = hsbkVec[i]->saturation;
                setColZon->brightness = hsbkVec[i]->brightness;
                setColZon->kelvin = hsbkVec[i]->kelvin;


                if (i == (numZones - 1)) {
                    setColZon->apply = APPLY;
                } else {
                    setColZon->apply = NO_APPLY;
                }


                //- send setColorZones - for each hsbk in hsbkvec
                header->ack_required = 1;
                header->pkt_type = SET_COLOR_ZONES;
                Response* resp4 = sendPacket<setColorZones>(header, setColZon, device, {STATE_MULTI_ZONE, ACKNOWLEDGEMENT});                                               
                if (resp4 == nullptr ) {
                    std::cout << "No response from device to setColorZones - (setMultiZonelight)" << std::endl;                                     
                    return false;
                }
                header->sequence++;

            }



            for (HSBK* hsb: hsbkVec) {
                delete(hsb);
            }
            delete(colZonResp->content);
            delete(colZonResp);



    } else if (extended_multizone) {

        int sizeOfColorArray = 82;



        // prepare, send, and recive getExtendedColorZOnes packet
        getExtendedColorZones* getEColZon = new getExtendedColorZones(); 
        header->pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* extColZonResp = sendPacket<nullPtr>(header, nullptr, device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (extColZonResp == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (setMultiZonelight)" << std::endl;
            return false;
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(extColZonResp->content));
        header->sequence++;
        int numZones = sECZ->zones_count;


        if (numZones != hsbkVec.size()) {
    
            std::cerr << "[-] number of zones of selected device does not match provided hsbk vector - (setMultiZonelight)" << std::endl;
            return false;
        } 


        int numMessages = ceil(static_cast<double>(hsbkVec.size()) / sizeOfColorArray);


        for (int i = 0; i < numMessages; i++) {
            
            int currStartIndex = i * sizeOfColorArray;
            setExtendedColorZones setEColZon = {};
            setEColZon.duration = duration;
            setEColZon.zone_index = currStartIndex;
            setEColZon.color_count = numZones;
            
            for (int x = 0; x < numZones; x++) {
                
                //setEColZon->colors[x] = hsbkVec[currStartIndex + x];
                setEColZon.colors[x].hue = hsbkVec[currStartIndex + x]->hue;
                setEColZon.colors[x].saturation = hsbkVec[currStartIndex + x]->saturation;
                setEColZon.colors[x].kelvin = hsbkVec[currStartIndex + x]->kelvin;
                setEColZon.colors[x].brightness = hsbkVec[currStartIndex + x]->brightness;
            } 


            if (i == (numMessages - 1)) {
                setEColZon.apply = APPLY;
            } else {
                setEColZon.apply = NO_APPLY;
            }
           

            //- send setExtendedColorZones,  
            header->ack_required = 1;
            header->pkt_type = SET_EXTENDED_COLOR_ZONES;
            Response* resp4 = sendPacket<setExtendedColorZones>(header, &setEColZon, device, {STATE_EXTENDED_COLOR_ZONES, ACKNOWLEDGEMENT});                                                        
            if (resp4 == nullptr ) {
                std::cout << "No response from device to setExtendedColorZones - (setMultiZonelight)" << std::endl;                                                                // shoudl probably try a couple more times before giving up
                return false;
            } 
            header->sequence++;

        }
        
        

        delete(extColZonResp->content);
        delete(extColZonResp);
        //for (HSBK* hsbk: hsbkVec){
        //    delete(hsbk);
        //}

        return true;

    } else {
        std::cerr << "[-] Unknow Error - (seeMultiZoneLight)" << std::endl; 
        return false;
    }
    return false;
}









/// @breif looks up lifx device in products.json and puts in extendedDeviceInfo struct
/// @param pid product id of lifx device
/// @param vid vendor id of lifx device
/// @param firmwareMinor the 
/// @param firmwareMajor
/// @return if device found returns extendedDeviceInfo fill with info, else return nullptr
extendedDeviceInfo* getExtendedDeviceInfo(lx_frame_t* header, lxDevice* device) {


    std::cout << "top of getextended device info" << std::endl; /////////////////////
     //- send getService, recieve stateService - to get proper port to use
    header->pkt_type = GET_SERVICE;
    Response* serviceResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_SERVICE});                                                     
    if (serviceResponse == nullptr) {
        std::cout << "[-] No response from device to getService - (gExtendedDeviceInfo)" << std::endl;
        return nullptr;
    }      
    stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(serviceResponse->content));
    device->port = htons(sS->port);
    header->sequence++;


    //- send getVersion, recive stateVersion - to get product and vendor id for lookup
    header->pkt_type = GET_VERSION;
    Response* versionResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_VERSION});                                                 
    if (versionResponse == nullptr) {
        std::cout << "No response from device to getVersion - (getExtendedDeviceInfo)" << std::endl;                   
        return nullptr;
    }
    stateVersion* sV = const_cast<stateVersion*>(reinterpret_cast<const stateVersion*>(versionResponse->content));
    header->sequence++;
   


    //- send getHostFirmware, reviece stateHostFirmware - to get firmware version for loopup
    header->pkt_type = GET_HOST_FIRMWARE;
    Response* firmwareResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_HOST_FIRMWARE});                      
    if (firmwareResponse == nullptr) {
        std::cout << "[-] No response from device to getHostFirmware - (getExtendedDeviceInfo)" << std::endl;                                           
        return nullptr;
    }
    stateHostFirmware* sHF = const_cast<stateHostFirmware*>(reinterpret_cast<const stateHostFirmware*>(firmwareResponse->content));
    header->sequence++;



    //- use the info from getHostFirmware and getVersion to queery products.json and get capabilities
    uint16_t firmwareMajor, firmwareMinor;    
    firmwareMajor = (sHF->version >> 16) & 0xFFF;
    firmwareMinor = sHF->version & 0xFFF;
   


    //- Load content of products.json into products JSON object
    std::ifstream inFile("products.json");
    if (!inFile) {
        throw std::runtime_error("Error: Unable to open JSON file");
    }

    nlohmann::json products;
    inFile >> products;
    inFile.close();

    bool found = false;
    nlohmann::json capabilities;
    std::string productName;

    //- Loop through vendors and load default capabilities
    for (const auto& vendor : products) {
        uint32_t vendor_vid = vendor["vid"].get<uint32_t>();
        
        if (vendor_vid != sV->vendor) {
            continue; 
        }


        if (!vendor.contains("defaults")) {
            std::cerr << "[-] defaults  missing for vendor VID: " << vendor_vid << "\n";
            continue;
        }
        capabilities = vendor["defaults"];

        if (!vendor.contains("products")) {
            std::cerr << "[Error] 'products' key missing for vendor VID: " << vendor_vid << "\n";
            continue;
        }

        //- Loop through products 
        for (const auto& product : vendor["products"]) {
            if (!product.contains("pid")) {
                std::cerr << "[Error] 'pid' key missing in one of the products for vendor VID: " << vendor_vid << "\n";
                continue;
            }

            uint32_t product_pid = product["pid"].get<uint32_t>();
            if (product_pid != sV->product) {
                continue;
            }


            productName = product["name"];
            capabilities.update(product["features"]);
            found = true;

            //- Check for upgraded features based on firmware version
            if (product.contains("upgrades")) {
                for (const auto& upgrade : product["upgrades"]) {
                    int upgrade_major = upgrade["major"];
                    int upgrade_minor = upgrade["minor"];

                    if (firmwareMajor > upgrade_major || 
                        (firmwareMajor == upgrade_major && firmwareMinor >= upgrade_minor)) {
                        capabilities.update(upgrade["features"]);
                    }
                }
            }

            break; 
        }

        if (found) {
            break; 
        }
    }

    if (!found) {
        std::cerr << "[-] Product not found - (getDeviceInfo)\n";
        return nullptr;
    }

    //- Fill in extendedDeviceInfo struct 
    extendedDeviceInfo* extDevInfo = new extendedDeviceInfo();
    extDevInfo->name = strdup(productName.c_str());
    extDevInfo->pid = sV->product;
    extDevInfo->vid = sV->vendor;
    extDevInfo->firmwareMinor = firmwareMinor;
    extDevInfo->firmwareMajor = firmwareMajor;

    if (capabilities.contains("hev")) {
        extDevInfo->hev = capabilities["hev"];
    }
    if (capabilities.contains("color")) {
        extDevInfo->color = capabilities["color"];
    }
    if (capabilities.contains("chain")) {
        extDevInfo->chain = capabilities["chain"];
    }
    if (capabilities.contains("matrix")) {
        extDevInfo->matrix = capabilities["matrix"];
    }
    if (capabilities.contains("relays")) {
        extDevInfo->relays = capabilities["relays"];
    }
    if (capabilities.contains("buttons")) {
        extDevInfo->buttons = capabilities["buttons"];
    }
    if (capabilities.contains("infrared")) {
        extDevInfo->infrared = capabilities["infrared"];
    }
    if (capabilities.contains("multizone")) {
        extDevInfo->multizone = capabilities["multizone"];
    }
    if (capabilities.contains("extended_multizone")) {
        extDevInfo->extended_multizone = capabilities["extended_multizone"];
    }
    if (capabilities.contains("temperature_range")) {
        auto range = capabilities["temperature_range"];
        extDevInfo->tempLowRange = range[0];
        extDevInfo->tempUpRange = range[1];
    }


    delete(serviceResponse->content);
    delete(versionResponse->content);
    delete(firmwareResponse->content);
    delete(serviceResponse);
    delete(versionResponse);
    delete(firmwareResponse);

    return extDevInfo;
}













/// @breif extract hsbk info from each zone of taret device and put into json document
/// 
///
/// 
///
///
bool saveScene(uint32_t ip, std::string path, std::string fileName, std::string sceneName) {


    lx_frame_t header = {};
    lxDevice device = {};
    in_addr addr = {};
    addr.s_addr = ip;


    header.protocol = 1024;
    header.addressable = 1;
    header.source = SOURCE;
    header.sequence = 0;
    header.size = HEADER_SIZE;

    header.pkt_type = GET_SERVICE;
    device.address = addr;                        
    device.port = htons(DEFAULT_LIFX_PORT);




    //- get extendedDeviceInfo to check what zones device supports
    extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
    if(extDevInfo == nullptr) {
        return false;
    }

    //- get vector of hsbk values for all zones
    std::vector<HSBK*> hsbkVec =  getMultiZoneData(&header, &device, extDevInfo->extended_multizone);
    if (hsbkVec.empty()) {
        return false;
    }


    //- insert hsbk values into json document
    nlohmann::json scene;
    for (int i = 0; i < hsbkVec.size(); i++) {
        nlohmann::json zone = {
            {"zoneNum", i},
            {"hue", hsbkVec[i]->hue},
            {"saturation", hsbkVec[i]->saturation},
            {"brightness", hsbkVec[i]->brightness},
            {"kelvin", hsbkVec[i]->kelvin}
        };

        scene[0]["scene"]["zones"].push_back(zone);
    }



    std::ofstream outFile(path);
    if (!outFile) {
        std::cerr << "Error: Unable to create file" << std::endl;
        return false;
    }

    outFile << scene.dump(4); 
    outFile.close();
    return true;








}






/// @brief set light to hsbk values extracted form json document
/// 
/// 
/// 
///
/// @return bool success
bool loadScene(uint32_t ip, std::string pathToScene, std::string sceneName, uint32_t duration) {




    lx_frame_t header = {};
    lxDevice device = {};
    in_addr addr = {};
    addr.s_addr = ip;


    header.protocol = 1024;
    header.addressable = 1;
    header.source = SOURCE;
    header.sequence = 0;
    header.size = HEADER_SIZE;

    header.pkt_type = GET_SERVICE;
    device.address = addr;                        
    device.port = htons(DEFAULT_LIFX_PORT);




    //- get extendedDeviceInfo to check what zones device supports
    extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
    if(extDevInfo == nullptr) {
        return false;
    }



    //- make sure device supports multiZone && get the number of zones
    int numZones = 0;
    if (extDevInfo->multizone && extDevInfo->extended_multizone) {


        //- send getExtendedColorZones, recive stateExtendedColorZones - for numZones
        header.pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* resp3 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (resp3 == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (loadScene)" << std::endl;    
            return false;
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(resp3->content));
        header.sequence++;
        numZones = sECZ->zones_count;


    
    } else if (extDevInfo->multizone && !extDevInfo->extended_multizone) {

        

        //- send getColorZones, recieve stateZone - for number of zones
        getColorZones gColZones = {};
        gColZones.start_index = 0;
        gColZones.end_index = 255;
        header.pkt_type = GET_COLOR_ZONES;
        Response* resp3 = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE});
        if (resp3 == nullptr) {
            std::cout << "No response from device to getColorZones - (loadScene)" << std::endl; 
            return false;
        }
        header.sequence++;

        if (resp3->message_type == STATE_ZONE) {  
 
            std::cerr << "[-] device selected " <<  extDevInfo->name << " - only displaying 1 color, not scene" << std::endl;
            return false;

        } else if (resp3->message_type == STATE_MULTI_ZONE) {
            stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(resp3->content));
            numZones = sMZ->zones_count;
        
        } else {
            
            std::cerr << "[-] An error occured while detecting number of zones in device - " << extDevInfo->name << std::endl;
            return false;
        }

    } else {

        std::cerr << "[-]Device: " << extDevInfo->name << " can not use multi-color scenes" << std::endl;
        return false;
    }




    //- retrieve the number of zones from the scene json to compare to selected device
    std::ifstream inFile(pathToScene);
    if (!inFile) {
        std::cerr << "Error: Unable to open file" << std::endl;
    }

    nlohmann::json scene;
    inFile >> scene;
    inFile.close();

    int numZonesJson = scene[0]["scene"]["zoneCount"];

    if (numZonesJson != numZones) {
        std::cerr << "[-] The scene you are tying to use is for a device with " << numZonesJson << " zones, the device selected - " << extDevInfo->name << " has " << numZones << " zones" << std::endl;
        return false;
    }




    //- extract hsbk values for each zone 
    std::vector<HSBK*> hsbkVec;
    for (const auto& zone : scene[0]["scene"]["zones"]) {
        HSBK* hsbk = new HSBK();
        hsbk->hue = zone["hue"];
        hsbk->saturation = zone["saturation"];
        hsbk->brightness = zone["brightness"];
        hsbk->kelvin = zone["kelvin"];
        hsbkVec.push_back(hsbk);
    }



    
    bool result =  setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo->extended_multizone);
    return result;

}








// Kelvin: range 2500° (warm) to 9000° (cool)
// set staturation to 0 to get rid of color

/// @breif change the kelvin value of light
bool setKelvin(bool broadcast, uint32_t ip, uint32_t duration, uint16_t kelvin) {
    lx_frame_t header = {};
    lxDevice device = {};
    setColor setCol = {};
 
    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
        if (addrs.empty()) {
            std::cerr << "No lifx devices on the network" << std::endl;
            return false;
        }
    } else {
        in_addr addr = {};
        addr.s_addr = ip;
        addrs.push_back(addr);
    }

    std::cout << "adders.size()" << addrs.size() << std::endl; /////////////////////
    for (int i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);

       
       


        std::cout << "before getextended device info" << std::endl; /////////////////////
        //- get extendedDeviceInfo to check what zones device supports
        extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
        if(extDevInfo == nullptr) {
            return false;
        }


        //- make sure action is valid for device
        if (!extDevInfo->tempUpRange.has_value() || !extDevInfo->tempLowRange.has_value()) {
            std::cout << "[-] selected device does not support kelvin values" << std::endl;    
            return false;
        }

        if (kelvin < extDevInfo->tempLowRange.value() || kelvin > extDevInfo->tempUpRange.value()) {
            std::cout << "[-] device selected does not support provided kelvin value" << std::endl;    
            std::cout << "\t\t ** Must be within [" << extDevInfo->tempLowRange.value() << ", " << extDevInfo->tempUpRange.value() << "]" << std::endl;    
            return false;
            
        }





            //- check if light has multi zone capabilites
            if (extDevInfo->multizone || extDevInfo->extended_multizone) {
            
                //- send getColorZones, reviece stateMultiZone or stateZone - use response to see if light is curetly set to multiple values or just 1
                getColorZones gColZones = {};
                gColZones.start_index = 0;
                gColZones.end_index = 255;
                header.pkt_type = GET_COLOR_ZONES;
                Response* getColResponse = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE});                                 
                if (getColResponse == nullptr) {
                    std::cout << "No response from device to getColorZones - (kelvin)" << std::endl;
                    return false;
                }
                header.sequence++;


                //- light currently more than 1 color
                if (getColResponse->message_type == STATE_MULTI_ZONE) {
                

                    //- get hsbk for all zones and change brightness 
                    std::vector<HSBK*> hsbkVec = getMultiZoneData(&header, &device, extDevInfo->extended_multizone);


                    for(HSBK* hsbk: hsbkVec) {
                        hsbk->kelvin = kelvin;
                        hsbk->saturation = 0;

                    }


                    // call function to set the light and pass hsbkVec                    
                    bool setKelvinResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo->extended_multizone);
                



                for (HSBK* hsbk: hsbkVec) {
                    delete(hsbk);
                }
                delete(getColResponse->content);
                delete(getColResponse);
                delete(extDevInfo);
                return setKelvinResult;

                }
            }


        //- light is either not multizone of only displaying 1 value 





        //- send get, recieve state - for current hsbk values
        header.pkt_type = GET;
        Response* resp4 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp4 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (setKelvin)" << std::endl;    
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp4->content));
        header.sequence++;



        //- send SetColor - change the kelvin
        header.pkt_type = SET_COLOR;
        setCol.color.saturation = 0;
        setCol.color.hue = s->color.hue;
        setCol.color.brightness = s->color.brightness;
        setCol.color.kelvin = kelvin;
        setCol.duration = duration;

        
        header.ack_required = 1;
        Response* resp3 = sendPacket<setColor>(&header, &setCol, &device, {STATE, ACKNOWLEDGEMENT});
        if (resp3 == nullptr) {
            std::cout << "[-]/2 No response from device to setColor packet - (setKelvin)" << std::endl;     
            return false;
        }
        state* s2 = const_cast<state*>(reinterpret_cast<const state*>(resp3->content));
        header.sequence++;
   
        delete(resp4->content);
        delete(resp4);
        delete(resp3->content);
        delete(resp3);
        delete(extDevInfo);

 
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }
    return true;  
}









/// @breif prints a bunch of info about a lifx device
/// @param braodcast bool if all devices
/// @param ip uint32 ip adress, 0 if broadcast
/// @return bool if sucess
bool printInfo(bool broadcast, uint32_t ip) {

    lx_frame_t header = {}; 
    lxDevice device = {};

    std::vector<in_addr> addrs;
    in_addr addr;
    if (broadcast) {
        addrs = getAllDevices();
        if (addrs.empty()) {
            std::cerr << "No lifx devices on the network" << std::endl;
            return false;
        }
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



        extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
        if(extDevInfo == nullptr) {
            return false;
        }
        std::string productName = std::string(extDevInfo->name);


        //- check if device has multizone capabilities and if its currently using them
        bool currMulti = false;
        int numZones = 0;
        if (extDevInfo->multizone || extDevInfo->extended_multizone) {
            
                //- send getColorZones, reviece stateMultiZone or stateZone - use response to see if light is curetly set to multiple values or just 1
                getColorZones gColZones = {};
                gColZones.start_index = 0;
                gColZones.end_index = 255;
                header.pkt_type = GET_COLOR_ZONES;
                Response* getColResponse = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE}); 
                if (getColResponse == nullptr) {
                    std::cout << "No response from device to getColorZones - (kelvin)" << std::endl;
                    return false;
                }
                header.sequence++;


                //- light currently 1 color
                if (getColResponse->message_type == STATE_MULTI_ZONE) { 
                    stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(getColResponse->content));
                    numZones = sMZ->zones_count;
                    currMulti = true;
                }

        }



        // send get, recive state - for label
        header.pkt_type = GET;
        Response* resp4 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp4 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (printInfo)" << std::endl;    
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp4->content));



        std::cout << "--------------------------------------------" << std::endl;
        std::cout << "-------------   Print Info    --------------" << std::endl;
        std::cout << "--------------------------------------------\n" << std::endl;
        std::cout << "----- Product Details" << std::endl;
        std::cout << "  Device Name: " << extDevInfo->name << std::endl;
        std::cout << "  Device Vecndor ID: " << extDevInfo->vid << std::endl;
        std::cout << "  Device Product ID: " << extDevInfo->pid << std::endl;
        std::cout << "  Device Firmware Major: " << extDevInfo->firmwareMajor << std::endl;
        std::cout << "  Device Firmware Minor: " << extDevInfo->firmwareMinor << std::endl;
    



        std::cout << "\n\n----- Device Features" << std::endl;
        if(extDevInfo->hev) std::cout << "  HEV: True" << std::endl;
        if(extDevInfo->color) std::cout << "  Color: True" << std::endl;
        if(extDevInfo->chain) std::cout << "  Chain: True" << std::endl;
        if(extDevInfo->matrix) std::cout << "  Matrix: True" << std::endl;
        if(extDevInfo->relays) std::cout << "  Relays: True" << std::endl;
        if(extDevInfo->buttons) std::cout << "  Buttons: True" << std::endl;
        if(extDevInfo->infrared) std::cout << "  Infrared: True" << std::endl;
        if(extDevInfo->multizone) std::cout << "  Multizone: True" << std::endl;
        if(extDevInfo->extended_multizone) std::cout << "  Extended Multizone: True" << std::endl;
        if(extDevInfo->tempLowRange.has_value()) std::cout << "  Minimum Kelvin: " << extDevInfo->tempLowRange.value() << std::endl;
        if(extDevInfo->tempUpRange.has_value()) std::cout << "  Maximum Kelvin: " << extDevInfo->tempUpRange.value() << std::endl;


        if (currMulti) {

            std::cout << "\n\n----- Current State" << std::endl;
            std::cout << "  Device Power: " << s->power;
            (s->power == 0) ? std::cout << " (Off)\n" : std::cout << " (On)\n";
            std::cout << "  State: Displaying Multiple Colors" << std::endl;
            std::cout << "  Number of Zones: " << numZones << std::endl;
            

        } else {
           std::cout << "\n\n----- Current State" << std::endl;
            std::cout << "  Device Power: " << s->power;
            (s->power == 0) ? std::cout << " (Off)\n" : std::cout << " (On)\n";
            std::cout << "  Device Hue: " << s->color.hue << std::endl;
            std::cout << "  Device Saturation: " << s->color.saturation << std::endl;
            std::cout << "  Device Brightness: " << s->color.brightness << std::endl;
            std::cout << "  Device Kelvin: " << s->color.kelvin << std::endl;
        }



        std::cout << "\n\n----- Device Details" << std::endl;
        std::cout << "  Device Label: " << s->label << std::endl;
        std::cout << "  Device Address: " << inet_ntoa(device.address) << std::endl;
        std::cout << "  Device Port: " << htons(device.port) << std::endl;
        std::cout << "  Device Mac: " << uint64ToMac(device.target) << std::endl;
        std::cout << "\n\n";        

        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
        delete(resp4->content);
        delete(resp4);

    }
    return true;

}






/// @breif send SetColor lifx message
/// @param braodcast bool if apply to all devices
/// @param ip uint32 ip address, or 0 if broadcast
/// @param duration length color change will take
/// @param either hex or rgb c-string
/// @return bool success
bool setColorF(bool broadcast, uint32_t ip, uint32_t duration, HSBK* hsbk) {

    
    lx_frame_t header = {};
    lxDevice device = {};
    setColor setCol = {};
    std::vector<in_addr> addrs;
    if (broadcast) {
        addrs = getAllDevices();
        if (addrs.empty()) {
            std::cerr << "No lifx devices on the network" << std::endl;
            return false;
        }
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


        //- get info about device and check if it supports colros
        extendedDeviceInfo* extDevInfo =  getExtendedDeviceInfo(&header, &device);
        if(extDevInfo == nullptr) {
            return false;
        }

        if (!extDevInfo->color) {
            std::cerr << "Selected device " << inet_ntoa(device.address) << " does not support colors" << std::endl;
            delete(extDevInfo);
            continue;
        }



        //- send get, recive state - for current hsbk
        header.pkt_type = GET;
        Response* resp1 = sendPacket<nullPtr>(&header, nullptr, &device, {STATE});
        if (resp1 == nullptr) {
            std::cout << "[-] No response from device to Get packet - (setcolor)" << std::endl;                                                 
            return false;
        }
        state* s = const_cast<state*>(reinterpret_cast<const state*>(resp1->content));
        header.sequence++;


        //- send setColor, recieve state
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
            std::cout << "[-]/2 No response from device to setColor packet - (setcolor)" << std::endl;                                      
            return false;
        }
        state* s2 = const_cast<state*>(reinterpret_cast<const state*>(resp2->content));
        header.sequence++;
    
        delete(extDevInfo);
        delete(resp1->content);
        delete(resp1);
        delete(resp2->content);
        delete(resp2);
        
        memset(&header, 0, sizeof(header));
        memset(&device, 0, sizeof(device));
    }
    delete(hsbk);
    return true; 
}





/// @brief genorates a random number and sets global source variable to it, called once at begining of program
void genSource() {
    
        std::random_device rd; 
        std::mt19937 gen(rd()); 
        std::uniform_int_distribution<> distr(0, (1e6 - 1)); 
        SOURCE = distr(gen);
}







/// @breif get up the structor of json so its ready to be filled with scene data
/// @param scene json object that will have data entered into it
/// @param scenePath, path to scene
/// @param numZones, number of zones
/// @return boolean success
bool initScene(nlohmann::json& scene, const std::string& scenePath, int numZones) {
   
     // Check if the file already exists
    std::ifstream file(scenePath);
    if (file.good()) {
        std::cerr << "[-] Error: File already exists" << std::endl;
        return false;
    }

    // Create the JSON structure
    scene = nlohmann::json::array({
        {
            {"scene", {
                {"sceneName", "testName"},
                {"zoneCount", numZones},
                {"zones", nlohmann::json::array()}
            }}
        }
    });

    return true;
}






/// @brief convert a c-string to uint16 digit
/// @param str string to be converted
/// @return uint16 result
uint16_t strToUint16(const char* str) {
    try {
        int value = std::stoi(str);

        // Check if the value is within the range of uint16_t
        if (value < 0 || value > std::numeric_limits<uint16_t>::max()) {
            throw std::range_error("Value out of range for uint16_t");
        }

        // Return the value as uint16_t
        return static_cast<uint16_t>(value);;
    } catch (const std::invalid_argument& e) {
        // If std::stoi can't convert, handle the invalid input
        throw std::range_error("Invalid input, unable to convert to uint16_t");
    } catch (const std::out_of_range& e) {
        // If the number is too large for an int
        throw std::range_error("Value out of range for int");
    }
    return 0;
}





/// @breif convert c-string to uint32
/// @param c-string to be converted
/// @return the uint32 result
uint32_t charToUint32(const char* str) {
    // Check if input string is null
    if (str == nullptr) {
         throw std::invalid_argument("Input string is null.");
    }

    // Initialize errno to 0 before conversion
    errno = 0;
    char* endPtr = nullptr;

    // Convert string to unsigned long using strtoul
    unsigned long temp = std::strtoul(str, &endPtr, 10);

    // Check for conversion errors
    if (errno == ERANGE || temp > std::numeric_limits<uint32_t>::max()) {
       throw std::out_of_range("Value out of range for uint32_t.");
    }

    // Check if the whole string was converted
    if (*endPtr != '\0') {
        throw std::invalid_argument("Invalid characters found in input string.");
    }

    // Assign the result
    return static_cast<uint32_t>(temp);
    
}






bool setColorZonesF(uint32_t ip, uint32_t duration, bool random, std::vector<HSBK*>& hsbkVec) {


    if (&hsbkVec == nullptr) {
        std::cerr << "Colors vector null" << std::endl;
        return false;
    }


    //- create structs and load ip(s)
    lx_frame_t header = {};
    lxDevice device = {};
    in_addr addr = {};
    addr.s_addr = ip;
    header.protocol = 1024;
    header.addressable = 1;
    header.source = SOURCE;
    header.sequence = 0;
    header.size = HEADER_SIZE;
    device.address = addr;                        
    device.port = htons(DEFAULT_LIFX_PORT);


    //- get extendedDeviceInfo to check what zones device supports
    extendedDeviceInfo* extDevInfo = getExtendedDeviceInfo(&header, &device);
    if(extDevInfo == nullptr) {
        return false;
    }


    //- get number of zones depending on if extended or normal multizone
    int numZones = 0;




    if (extDevInfo->extended_multizone) {


        int sizeOfColorArray = 82;
        //- send getExtendedColorZones, recieve stateExtendedColorZones - to get number of zones 
        getExtendedColorZones* getEColZon = new getExtendedColorZones(); 
        header.pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* extColZonResp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (extColZonResp == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (setColorZonesF)" << std::endl;
            return false;
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(extColZonResp->content));
        header.sequence++;
        numZones = sECZ->zones_count;
        //if (numZones != hsbkVec.size()) {
        //    std::cerr << "[-] number of zones of selected device does not match provided hsbk vector - (setColorZonesF)" << std::endl;
        //    return false;
        //}   
        delete(extColZonResp->content);
        delete(extColZonResp);



    } else if (extDevInfo->multizone) {


        //- send getColorZones - to get number of zones
        getColorZones gColZones = {};
        gColZones.start_index = 0;
        gColZones.end_index = 255;
        header.pkt_type = GET_COLOR_ZONES;
        Response* colZonResp = sendPacket<getColorZones>(&header, &gColZones, &device, {STATE_ZONE, STATE_MULTI_ZONE});             
        if (colZonResp == nullptr) {
            std::cout << "No response from device to getColorZones - (setColorZonesF)" << std::endl;
            return false;
        }
        header.sequence++;
        if (colZonResp->message_type == STATE_ZONE) {  
            stateZone* sZ = const_cast<stateZone*>(reinterpret_cast<const stateZone*>(colZonResp->content));
            numZones = sZ->zone_count;
        } else if (colZonResp->message_type == STATE_MULTI_ZONE) {
            stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(colZonResp->content));
            numZones = sMZ->zones_count;
        } else {
            std::cout << "[-] unexpected response from getColorZones - (setColorZonesf) " << std::endl;
            return false;
        }
        delete(colZonResp->content);
        delete(colZonResp);

    } else {
        std::cerr << "Selected Device Does cannot display multiple colors" << std::endl;
        return false;
    }


    if (random) {


        // create new hsbkVec and randomly push hsbk's from old vec in until numZones
        std::vector<HSBK*> finalVec;
        std::random_device rd; 
        std::mt19937 gen(rd()); 
        std::uniform_int_distribution<> distr(0, (hsbkVec.size() - 1)); 
        for (int i = 0; i < numZones; i++) {
            int randIndex = distr(gen);
            finalVec.push_back(hsbkVec[randIndex]);
        }
        

        bool setColResult = setMultiZoneLight(&header, &device, duration, finalVec, extDevInfo->extended_multizone);
        if (!setColResult) {
            return false;
        }

    } else {

        if (hsbkVec.size() != numZones) {
            std::cerr << "Number of colors provided does not match number of zones of light" << std::endl;
            std::cerr << "Use random if you do not want to display exact colors" << std::endl;
            return false;
        }


        bool setColResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo->extended_multizone);
        if (!setColResult) {
            return false;
        }
    }

    for (HSBK* hsbk : hsbkVec) {
        delete(hsbk);
    }
    delete(&hsbkVec);
    delete(extDevInfo);
    return true;


}














// takes a c-string forn user input in the form [0xffffff,0xaaaaaa,0xbbbbbb]
// return a heap allocated vector containing hsbk representation of each value in 
std::vector<HSBK*>* strToHSBKVec(const char* colorInput) {


    std::string hexStr = std::string(colorInput);
    std::vector<HSBK*>* hsbkVec = new std::vector<HSBK*>();


    if (hexStr.front() != '[' && hexStr.back() != ']') {
        std::cerr << "[-] Provided colors not formatted correctly - 1" << std::endl;
        std::cerr << "User -h for proper usage" << std::endl;
        return {};
    }

    // remove the [
    hexStr = hexStr.substr(1);
    std::string tempStr;

    while (true) {

        int R, G, B;
        if (hexStr.length() >= 9 && hexStr.front() == '0' && hexStr[1] == 'x' && (hexStr[8] == ',' || hexStr[8] == ']')) {
            hexStr = hexStr.substr(2);  // take off 0x
        
            try {
                R = std::stoi(hexStr.substr(0, 2), nullptr, 16); // R
                G = std::stoi(hexStr.substr(2, 2), nullptr, 16); // G
                B = std::stoi(hexStr.substr(4, 2), nullptr, 16); // B
                hexStr = hexStr.substr(6);  // take off the 6 hex digits
            } catch (const std::exception& e) {
                std::cout << "[-] Error converting hex to RGB: " << e.what() << " - (strToHSBKVec)" << std::endl;
                return {};
            }
        } else {
            std::cerr << "[-] Provided colors not formatted correctly - 2" << std::endl;
            std::cerr << "User -h for proper usage" << std::endl;
            return {};
        
        }


        double r = R / 255.0;
        double g = G / 255.0;
        double b = B / 255.0;

        double maxVal = fmax(r, fmax(g, b));
        double minVal = fmin(r, fmin(g, b));
        double delta = maxVal - minVal;


        HSBK* hsbk = new HSBK();

        // Calculate Hue (0 - 65535)
        if (delta == 0) {
            hsbk->hue = 0;
        } else if (maxVal == r) {
            hsbk->hue = (int)(60 * ((g - b) / delta + (g < b ? 6 : 0)) * 182.04); // Scale to 0-65535
        } else if (maxVal == g) {
            hsbk->hue = (int)(60 * ((b - r) / delta + 2) * 182.04);
        } else {
            hsbk->hue = (int)(60 * ((r - g) / delta + 4) * 182.04);
        }

        // Calculate Saturation (0 - 65535)
        hsbk->saturation = (maxVal == 0) ? 0 : (int)((delta / maxVal) * 65535);
        // Calculate Brightness (0 - 65535)
        hsbk->brightness = (int)(maxVal * 65535);
        // Default Kelvin (LIFX default: 3500K for colors)
        hsbk->kelvin = 3500;
        hsbkVec->push_back(hsbk);



        if (hexStr.front() == ']') {
            break;
        } else if (hexStr.front() == ',') {
            hexStr = hexStr.substr(1);
            continue;
        } else {
            std::cerr << "[-] Provided colors not formatted correctly -3" << std::endl;
            std::cerr << "User -h for proper usage" << std::endl;
            return {};
        }
    }

    return hsbkVec;
}






























/// @breif print the help page
/// @return void
void printHelp() {


    std::cout << "Usage: " << std::endl;
    std::cout << "  Target device must be specified, wither specifically with --ip, or --all if " << std::endl;
    std::cout << "  you want the command applie to all lifx devices on the network. Use --list " << std::endl;
    std::cout << "  to view all lifx devices on your network. " << std::endl;


    std::cout << "Commands: " << std::endl;
    std::cout << "  --on:                           turns a light on            " << std::endl;
    std::cout << "  -o:                               can be used with --all          " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --off:                          turns a light off            " << std::endl;
    std::cout << "  -f:                               can be used with --all          " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --setColor:                     sets a light to a single color provided            " << std::endl;
    std::cout << "  -c:                               as hex value, see color format section          " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --setColors:                    sets each zone of light to corresplonding hex             " << std::endl;
    std::cout << "  -s:                               provided. Number of colors provided must match number of zones.   " << std::endl;
    std::cout << "                                    only avalible for multi-zone lights. --info for number of zones" << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --setColorsR:                               " << std::endl;
    std::cout << "  -r:                                         " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --brightness:                               " << std::endl;
    std::cout << "  -b:                                         " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --warmth:                                   " << std::endl;
    std::cout << "  -w:                                         " << std::endl;
    std::cout << "      " << std::endl;


    
    std::cout << "Queery" << std::endl;
    std::cout << "  --list: " << std::endl;
    std::cout << "  -l: " << std::endl;
    std::cout << "      " << std::endl;
    std::cout << "  --info: " << std::endl;
    std::cout << "  -i: " << std::endl;
    std::cout << "      " << std::endl;


    std::cout << "Setter" << std::endl;
    std::cout << "  --ip:                           ipv4 dot notaion of target lifx device          " << std::endl;
    std::cout << "  -p:                                addresses can be obtainted with --list       " << std::endl;
    std::cout << "                                                                                  " << std::endl;
    std::cout << "  --all:                          sets target to all lirx device on network       " << std::endl;
    std::cout << "  -a:                                not avalible for all commands                " << std::endl;
    std::cout << "                                                                                  " << std::endl;
    std::cout << "  --duration:                     sets how long an action will take to complete   " << std::endl;
    std::cout << "  -d:                                not avalible for all commands                " << std::endl;
    std::cout << "                                                                                  " << std::endl;
    

    std::cout << "Color Format" << std::endl;
    std::cout << "  All collors passed by user should be hex valules inside of brackets sepperated" << std::endl;
    std::cout << "  by commas with no spaces between" << std::endl;
    std::cout << "  Ex.  " << std::endl;
    std::cout << "    lifx -setColor [0x2e6f40] --ip 192.168.8.141  " << std::endl;
    std::cout << "    lifx -setColorsR [0x2e6f40,0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0]  --ip 192.168.8.141" << std::endl;




}











// -on          -o 
// -off         -f
// -setColor    -c
// -list        -l
// -info        -i
// -brightness  -b
// -warmth      -w
// -setColors   -s
// -setColorsR  -r
// -ip          -p
// all          -a
// -duration    -d
// -help        -h




bool parseArgs(int argc, char** argv, inputArgs* options) {

    

    const char* const shortOpts = "hofs:lib:w:c:r:p:ad:";
    const option longOpts[] = {
 
    {"help", no_argument, nullptr, 'h'},
    {"on", no_argument, nullptr, 'o'},
    {"off", no_argument, nullptr, 'f'},
    {"setColor", required_argument, nullptr, 'c'},
    {"list", no_argument, nullptr, 'l'},
    {"info", no_argument, nullptr, 'i'},
    {"brightness", required_argument, nullptr, 'b'},
    {"warmth", required_argument, nullptr, 'w'},
    {"setColors", required_argument, nullptr, 's'},
    {"setColorsR", required_argument, nullptr, 'r'},
    {"ip", required_argument, nullptr, 'p'},
    {"all", no_argument, nullptr, 'a'},
    {"duration", required_argument, nullptr, 'd'},
    {0, 0, 0, 0}

    };


    while(true) {

        const auto opt = getopt_long(argc, argv, shortOpts, longOpts, nullptr);
        
        if (opt == -1) {
            break;
        }

       switch(opt) {

        case 'h': // help
            options->help = true;
            return true;
            break;
        case 'o': // on
            options->brightVal = 65535;
            options->on = true;
            break;
        case 'f': // off
            options->brightVal = 0;
            options->off = true;  
            break;
        case 'c': // setColor
            if (optarg) {
                std::vector<HSBK*>* hsbkVec = strToHSBKVec(optarg);
                if (hsbkVec->empty()) {
                    return false;
                } else if (hsbkVec->size() > 1) {
                    std::cerr << "[-] setColor only takes one color, multiple provided" << std::endl;
                    std::cerr << "\nUse -h for usage information" << std::endl;
                    return false;
                }
                options->color = (*hsbkVec)[0];
                delete(hsbkVec);
                options->setCol = true;
            } else {
                std::cerr << "[-] Missing argument for -c / --setColor" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            break;
         case 'l':  // list
            options->list = true;
            break;
        case 'i':
            options->info = true;
            break;
         case 'b': //brightness
            if (optarg) {
                try {
                    uint16_t tempBrightness = strToUint16(optarg);
                    if (tempBrightness > 100) {
                        std::cout << "[-] invalid brightness, value entered " << optarg  << std::endl;
                        std::cerr << "\nUse -h for usage information" << std::endl;
                        return false;
                    }
                    
                    float tempBrightness2 = static_cast<float>(tempBrightness) / 100.0f;
                    uint16_t brightness = static_cast<uint16_t>(65535 * tempBrightness2);
                    options->brightVal = brightness;
                } catch (const std::range_error& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                    std::cout << "[-] Invalid duration value: " << optarg  << std::endl;
                    return -1;
                }
            } else {
                std::cerr << "[-] Missing argument for -b / --brightness" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            options->brightness = true;
            break;
        case 'w':
            if (optarg) {
                uint16_t kelvin = 0;              
                 try {
                    kelvin = strToUint16(optarg);
                } catch (const std::range_error& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                    std::cout << "[-] Invalid warmth value: " << optarg << "     - (main/warmth)" << std::endl;
                    return -1;
                }
            options->kelvin = kelvin;
            } else {
                std::cerr << "[-] Missing argument for -w / --warmth" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            options->warmth = true;
            break;
         case 's': // setColors
            if (optarg) {
                std::vector<HSBK*>* hsbkVec = strToHSBKVec(optarg);
                if (hsbkVec->empty()) {
                    return false; 
                } 
                options->colsVec = hsbkVec;
                options->setCols = true;
            } else {
                std::cerr << "[-] Missing argument for -s / --setColors" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            break;
        case 'r': 
            if (optarg) { 
                std::vector<HSBK*>* hsbkVec = strToHSBKVec(optarg);
                if (hsbkVec->empty()) {
                    return false; 
                } 
                options->colsRVec = hsbkVec;
                options->setColsR = true;
            } else {
                std::cerr << "[-] Missing argument for -r / --setColorsR" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            break;
         case 'p': // ip
            if (optarg) { 
                in_addr deviceIp;
                if (!inet_aton(optarg, &deviceIp)) {
                    std::cout << "[-] IP address invalid: " << optarg << ", use -help for proper usage - (m)" << std::endl;
                    return -1;
                }
                options->address = deviceIp.s_addr;
            } else {
                std::cerr << "[-] Missing argument for -p / --ip" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            options->ip = true;
            break;
        case 'a': // all
            options->all = true;
            break;
         case 'd': // duration 
            if (optarg) {
                try {
                    options->durVal = charToUint32(optarg);
                } catch (const std::range_error& e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                    std::cout << "[-] Invalid duration value: " << optarg << std::endl;
                    return -1;
                }
            } else {
                std::cerr << "[-] Missing argument for -d / --duration" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            options->duration = true;
            break;
        default: /*  ?  */
            if (argv[optind - 1][0] == '-' && argv[optind - 1][1] != '\0') {
                std::cerr << "[-] Unknown or invalid option: " << argv[optind - 1] << std::endl;
                std::cerr << "Use -h for usage information" << std::endl;
            } else {
                std::cerr << "[-] Unknown option or missing argument" << std::endl;
            }
            return false;

        } 
    }


    if (optind < argc) {
        std::cerr << "[-] non-recognized argumetns: ";
        while (optind < argc) {
            std::cerr << argv[optind++] << " ";
        }
        std::cerr << "\nUse -h for usage information" << std::endl;
        return false;
    }
        
    return true;
}















/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */







int main(int argc, char** argv) {


    //- parse args and fill out inputAtgs struct
    inputArgs* options = new inputArgs();
    options->address = 0;                               //see if I can do in .h
    if (!parseArgs(argc, argv, options)) return -1;



    if (options->help) {
        printHelp();
        return 0;
    } 

    else if (options->list) {
        listDevices();
        return 0;
    }






    if (options->all && options->ip) {
        std::cerr << "Cannot select all and specific ip at once" << std::endl;
        std::cerr << "Use -h for usage information" << std::endl;
        return -1;
    }

    if (!options->all && !options->ip) {
        std::cerr << "No Device Selected" << std::endl;
        std::cerr << "Use -h for usage information" << std::endl;
        return -1;
    }



    uint16_t on = 65535;
    uint16_t off = 0;
    // main loop 




    if (options->info) {
        printInfo(options->all, options->address); 
        return 0;
    }


    else if (options->on) {
        if (setPower(options->all, false, options->address, on, options->durVal)) return 0;
        return -1;
    }

    else if (options->off) {
        if (setPower(options->all, false, options->address, off, options->durVal)) return 0;
        return -1;
    }

    else if (options->setCol) {
        if (setColorF(options->all, options->address, options->durVal, options->color)) return 0;

    }

    else if (options->setCols) {
        if (options->all) {
            std::cerr << "setColorsRandom cannot be performed on --all" << std::endl;
            std::cerr << "Use -h for usage information" << std::endl;
            return -1;
        }
        if (setColorZonesF(options->address, options->durVal, false, *(options->colsVec))) return 0;
        return -1;
    }

    else if (options->setColsR) {
        if (options->all) {
            std::cerr << "setColorsRandom cannot be performed on --all" << std::endl;
            std::cerr << "Use -h for usage information" << std::endl;
            return -1;
        }
        if (setColorZonesF(options->address, options->durVal, true, *(options->colsRVec))) return 0;
        return -1;
    }

    else if (options->brightness) {
        if (setPower(options->all, true, options->address, options->brightVal, options->durVal)) return 0;
        return -1;
        
    }

    else if (options->warmth) {
        
        if (setKelvin(options->all, options->address, options->durVal, options->kelvin)) return 0;
        return -1;
    }





















    return 0;    
}





















