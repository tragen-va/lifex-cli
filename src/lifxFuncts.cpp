// lifxFuncts.cpp



/*
#include "structsAndDefs.h"
#include <unistd.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <optional>
#include "json.hpp"
*/




#include "lifxFuncts.h"
#include <arpa/inet.h>         // for htons, inet_ntoa
#include <netinet/in.h>        // for in_addr
#include <algorithm>           // for max, min
#include <cmath>               // for ceil, floor
#include <cstring>             // for memset, strdup
#include <filesystem>          // for create_directory, exists, path
#include <fstream>             // for basic_ostream, operator<<, endl, basic...
#include <iomanip>             // for operator<<, setw
#include <iostream>            // for cout, cerr
#include <optional>            // for optional
#include <random>              // for random_device, uniform_int_distribution
#include <string>              // for char_traits, basic_string, operator<<
#include <tuple>               // for get, tuple
#include <vector>              // for vector
#include "helperFuncts.h"      // for getScenes, uint64ToMac, initScene
#include "json.hpp"            // for basic_json, json_ref, json, operator>>
#include "networkComFuncts.h"  // for sendPacket, getAllDevices, sendMultiPa...
#include "structsAndDefs.h"    // for HSBK, Response, stateMultiZone, STATE_...











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
    for (size_t i = 0; i < addrs.size(); i++) {

        //- setup header
        header.protocol = 1024;
        header.addressable = 1;
        header.pkt_type = GET_SERVICE;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        device.address = addrs[i];
        device.port = htons(DEFAULT_LIFX_PORT);
    

        extendedDeviceInfo extDevInfo = {};
        if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;
        std::string productName = std::string(extDevInfo.name);



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

    for (size_t i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        header.pkt_type = GET_SERVICE;
        device.address.s_addr = addrs[i].s_addr;
        device.port = htons(DEFAULT_LIFX_PORT);
       

        extendedDeviceInfo extDevInfo = {};
        if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;


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
            


        //- if change brightness
        } else if (brightness) {


            //- check if light has multi zone capabilites
            if (extDevInfo.multizone || extDevInfo.extended_multizone) {
            
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
                header.sequence++;


                //- light currently more than 1 color
                if (getColResponse->message_type == STATE_MULTI_ZONE) {
                




                    //- get hsbk for all zones and change brightness 
                    std::vector<HSBK*> hsbkVec = getMultiZoneData(&header, &device, extDevInfo.extended_multizone);
                    for(HSBK* hsbk: hsbkVec) {
                        hsbk->brightness = level;
                    }


                    // call function to set the light and pass hsbkVec                    
                    bool setLightResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo.extended_multizone);
                



                for (HSBK* hsbk: hsbkVec) {
                    delete(hsbk);
                }
                delete(getColResponse->content);
                delete(getColResponse);
                return setLightResult;

                } else {
                    std::cerr << "[-] An unexpected error occured in setPow" << std::endl;
                    return false;
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
            numZones = sMZ->zones_count;
            
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
            for (size_t i = 0; i < responses.size(); i++) {
                for (int x = ((stateMultiZone*)responses[i]->content)->zone_index; x < numZones; x++) {
                    hsbkVec.push_back(&((stateMultiZone*)responses[i]->content)->colors[x]);
                    HSBK* hsbk = new HSBK();
                    hsbk->hue = ((stateMultiZone*)responses[i]->content)->colors[x].hue;
                    hsbk->saturation = ((stateMultiZone*)responses[i]->content)->colors[x].saturation;
                    hsbk->brightness = ((stateMultiZone*)responses[i]->content)->colors[x].brightness;
                    hsbk->kelvin = ((stateMultiZone*)responses[i]->content)->colors[x].kelvin;
                }
            }


        return hsbkVec;
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

        int numZones = 0;
        int numMessages = 0;


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
        numZones = sECZ->zones_count;
        numMessages = ceil(sECZ->zones_count / sizeOfColorArray);
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
            


            if (numZones != static_cast<int>(hsbkVec.size())) {
        
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
        header->pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* extColZonResp = sendPacket<nullPtr>(header, nullptr, device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (extColZonResp == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (setMultiZonelight)" << std::endl;
            return false;
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(extColZonResp->content));
        header->sequence++;
        int numZones = sECZ->zones_count;


        if (numZones != static_cast<int>(hsbkVec.size())) {
    
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
bool getExtendedDeviceInfo(lx_frame_t* header, lxDevice* device, extendedDeviceInfo& extDevInfo) {


     //- send getService, recieve stateService - to get proper port to use
    header->pkt_type = GET_SERVICE;
    Response* serviceResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_SERVICE});                                                     
    if (serviceResponse == nullptr) {
        std::cout << "[-] No response from device to getService - (gExtendedDeviceInfo)" << std::endl;
        return false;
    }      
    stateService* sS = const_cast<stateService*>(reinterpret_cast< const stateService*>(serviceResponse->content));
    device->port = htons(sS->port);
    header->sequence++;


    //- send getVersion, recive stateVersion - to get product and vendor id for lookup
    header->pkt_type = GET_VERSION;
    Response* versionResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_VERSION});                                                 
    if (versionResponse == nullptr) {
        std::cout << "No response from device to getVersion - (getExtendedDeviceInfo)" << std::endl;                   
        return false;
    }
    stateVersion* sV = const_cast<stateVersion*>(reinterpret_cast<const stateVersion*>(versionResponse->content));
    header->sequence++;
   


    //- send getHostFirmware, reviece stateHostFirmware - to get firmware version for loopup
    header->pkt_type = GET_HOST_FIRMWARE;
    Response* firmwareResponse = sendPacket<nullPtr>(header, nullptr, device, {STATE_HOST_FIRMWARE});                      
    if (firmwareResponse == nullptr) {
        std::cout << "[-] No response from device to getHostFirmware - (getExtendedDeviceInfo)" << std::endl;                                           
        return false;
    }
    stateHostFirmware* sHF = const_cast<stateHostFirmware*>(reinterpret_cast<const stateHostFirmware*>(firmwareResponse->content));
    header->sequence++;



    //- use the info from getHostFirmware and getVersion to queery products.json and get capabilities
    uint16_t firmwareMajor, firmwareMinor;    
    firmwareMajor = (sHF->version >> 16) & 0xFFF;
    firmwareMinor = sHF->version & 0xFFF;
   


    //- Load content of products.json into products JSON object
    std::ifstream inFile(PATH_TO_PRODUCTS);
    if (!inFile) {
        std::cerr << "[-] Error Unable to open JSON file, path: " << PATH_TO_PRODUCTS  << std::endl;
        return false;
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
        return false;
    }

    //- Fill in extendedDeviceInfo struct 
    extDevInfo.name = strdup(productName.c_str());
    extDevInfo.pid = sV->product;
    extDevInfo.vid = sV->vendor;
    extDevInfo.firmwareMinor = firmwareMinor;
    extDevInfo.firmwareMajor = firmwareMajor;

    if (capabilities.contains("hev")) {
        extDevInfo.hev = capabilities["hev"];
    }
    if (capabilities.contains("color")) {
        extDevInfo.color = capabilities["color"];
    }
    if (capabilities.contains("chain")) {
        extDevInfo.chain = capabilities["chain"];
    }
    if (capabilities.contains("matrix")) {
        extDevInfo.matrix = capabilities["matrix"];
    }
    if (capabilities.contains("relays")) {
        extDevInfo.relays = capabilities["relays"];
    }
    if (capabilities.contains("buttons")) {
        extDevInfo.buttons = capabilities["buttons"];
    }
    if (capabilities.contains("infrared")) {
        extDevInfo.infrared = capabilities["infrared"];
    }
    if (capabilities.contains("multizone")) {
        extDevInfo.multizone = capabilities["multizone"];
    }
    if (capabilities.contains("extended_multizone")) {
        extDevInfo.extended_multizone = capabilities["extended_multizone"];
    }
    if (capabilities.contains("temperature_range")) {
        auto range = capabilities["temperature_range"];
        extDevInfo.tempLowRange = range[0];
        extDevInfo.tempUpRange = range[1];
    }


    delete(serviceResponse->content);
    delete(versionResponse->content);
    delete(firmwareResponse->content);
    delete(serviceResponse);
    delete(versionResponse);
    delete(firmwareResponse);

    return true;
}





/// @breif extract hsbk info from each zone of taret device and put into json document
/// 
///
/// 
///
///
bool saveScene(uint32_t ip, std::string sceneName) {


    // Check if PATH_TO_SCENES exists, create it if not
    if (!std::filesystem::exists(PATH_TO_SCENES)) {
        if (!std::filesystem::create_directory(PATH_TO_SCENES)) {
            std::cerr << "Error: Failed to create directory " << PATH_TO_SCENES << std::endl;
            return false;
        }
    }




    //- check if sceneName entered has .json at end, if not add it. and append to path
    if (sceneName.size() < 6) {
        sceneName.append(".json");
    } else {
        std::string extention = sceneName.substr(sceneName.size() - 5);
        if (extention != ".json") {
            sceneName.append(".json");
        }
    }
    std::string filePath = std::string(PATH_TO_SCENES);
    filePath.append(sceneName);



    
    //- get all scens and their numZones
    std::vector<std::tuple<std::string, int>> scenes = getScenes(PATH_TO_SCENES);
    if (!scenes.empty()) {
        //- check if coulnt open directory
        if (std::get<0>(scenes[0]) == "error" && std::get<1>(scenes[0]) == 0) {
            return false;
        }
        for (size_t i = 0; i < scenes.size(); i++) {
            if (std::get<0>(scenes[i]) == sceneName) {
                std::cerr << sceneName << " already exists, choise a unique name of use --deleteScene" << std::endl;
                return false;
            }
        }
    }





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
    extendedDeviceInfo extDevInfo = {};
    if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;



    //- get vector of hsbk values for all zones
    std::vector<HSBK*> hsbkVec =  getMultiZoneData(&header, &device, extDevInfo.extended_multizone);
    if (hsbkVec.empty()) {
        return false;
    }


    //- get numZones
    int numZones = 0;
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
        stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(getColResponse->content));
        numZones = sMZ->zones_count;
    } else {
        return false; // only 1 color
    }




    //- insert hsbk values into json document
    nlohmann::json scene;
    bool initSceneResult = initScene(scene, filePath, numZones);
    if (!initSceneResult) {
        std::cerr << "Could not initilize scene" << std::endl;
        return false;
    }


    for (size_t i = 0; i < hsbkVec.size(); i++) {
        nlohmann::json zone = {
            {"zoneNum", i},
            {"hue", hsbkVec[i]->hue},
            {"saturation", hsbkVec[i]->saturation},
            {"brightness", hsbkVec[i]->brightness},
            {"kelvin", hsbkVec[i]->kelvin}
        };

        scene[0]["scene"]["zones"].push_back(zone);
    }



    std::ofstream outFile(filePath);
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
bool loadScene(uint32_t ip, std::string sceneName, uint32_t duration) {


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
    extendedDeviceInfo extDevInfo = {};
    if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;


    //- make sure device supports multiZone && get the number of zones
    int numZones = 0;
    if (extDevInfo.multizone && extDevInfo.extended_multizone) {


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


    
    } else if (extDevInfo.multizone && !extDevInfo.extended_multizone) {

        

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
 
            std::cerr << "[-] device selected " <<  extDevInfo.name << " - only displaying 1 color, not scene" << std::endl;
            return false;

        } else if (resp3->message_type == STATE_MULTI_ZONE) {
            stateMultiZone* sMZ = const_cast<stateMultiZone*>(reinterpret_cast<const stateMultiZone*>(resp3->content));
            numZones = sMZ->zones_count;
        
        } else {
            
            std::cerr << "[-] An error occured while detecting number of zones in device - " << extDevInfo.name << std::endl;
            return false;
        }

    } else {

        std::cerr << "[-]Device: " << extDevInfo.name << " can not use multi-color scenes" << std::endl;
        return false;
    }


    //- check if sceneName entered has .json at end, if not add it
    if (sceneName.size() < 6) {
        sceneName.append(".json");
    } else {
        std::string extention = sceneName.substr(sceneName.size() - 5);
        if (extention != ".json") {
            sceneName.append(".json");
        }
    }



    //- get all scens and their numZones
    std::vector<std::tuple<std::string, int>> scenes = getScenes(PATH_TO_SCENES);
    if (scenes.empty()) {
        std::cerr << "[-] Unable to find any saved scenes from " << PATH_TO_SCENES << std::endl;
        return false;
    }
    if (std::get<0>(scenes[0]) == "error" && std::get<1>(scenes[0]) == 0) {
        std::cerr << "[-] An error occured while trying to access the files at " << PATH_TO_SCENES << std::endl;
        return false;
    } 
    bool exists = false;
    for (size_t i = 0; i < scenes.size(); i++) {
        if (std::get<0>(scenes[i]) == sceneName) {
            exists = true;
        }
    }
    if (!exists) {
        std::cerr << "[-] The scene name provided does not match a file at " << PATH_TO_SCENES << std::endl;
    }



    //- retrieve the number of zones from the scene json to compare to selected device
    std::string file = std::string(PATH_TO_SCENES);
    file.append(sceneName);
    std::ifstream inFile(file);
    if (!inFile) {
        std::cerr << "Error: Unable to open file" << std::endl;
    }

    nlohmann::json scene;
    inFile >> scene;
    inFile.close();

    int numZonesJson = scene[0]["scene"]["zoneCount"];

    if (numZonesJson < numZones) {
        std::cerr << "[-] The scene you are tying to use is for a device with at least" << numZonesJson << " zones, the device selected - " << extDevInfo.name << " only has " << numZones << " zones" << std::endl;
        return false;
    } 



    //- extract hsbk values for each zone 
    std::vector<HSBK*> hsbkVec;
    for (int i = 0; i < numZones; i++) {
        HSBK* hsbk = new HSBK();
        hsbk->hue = scene[0]["scene"]["zones"]["hue"];
        hsbk->saturation = scene[0]["scene"]["zones"]["saturation"];
        hsbk->brightness = scene[0]["scene"]["zones"]["brightness"];
        hsbk->kelvin = scene[0]["scene"]["zones"]["kelvin"];
        hsbkVec.push_back(hsbk);
    }


    
    bool result =  setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo.extended_multizone);
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

    for (size_t i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);

       

        //- get extendedDeviceInfo to check what zones device supports
        extendedDeviceInfo extDevInfo = {};
        if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;


        //- make sure action is valid for device
        if (!extDevInfo.tempUpRange.has_value() || !extDevInfo.tempLowRange.has_value()) {
            std::cout << "[-] selected device does not support kelvin values" << std::endl;    
            return false;
        }

        if (kelvin < extDevInfo.tempLowRange.value() || kelvin > extDevInfo.tempUpRange.value()) {
            std::cout << "[-] device selected does not support provided kelvin value" << std::endl;    
            std::cout << "\t\t ** Must be within [" << extDevInfo.tempLowRange.value() << ", " << extDevInfo.tempUpRange.value() << "]" << std::endl;    
            return false;
            
        }





            //- check if light has multi zone capabilites
            if (extDevInfo.multizone || extDevInfo.extended_multizone) {
            
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
                    std::vector<HSBK*> hsbkVec = getMultiZoneData(&header, &device, extDevInfo.extended_multizone);


                    for(HSBK* hsbk: hsbkVec) {
                        hsbk->kelvin = kelvin;
                        hsbk->saturation = 0;

                    }


                    // call function to set the light and pass hsbkVec                    
                    bool setKelvinResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo.extended_multizone);
                



                for (HSBK* hsbk: hsbkVec) {
                    delete(hsbk);
                }
                delete(getColResponse->content);
                delete(getColResponse);
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
        header.sequence++;
   
        delete(resp4->content);
        delete(resp4);
        delete(resp3->content);
        delete(resp3);
        

 
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

    for (size_t i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE;
        device.address.s_addr = addrs[i].s_addr;
        header.pkt_type = GET_SERVICE;
        device.port = htons(DEFAULT_LIFX_PORT);



        
        extendedDeviceInfo extDevInfo = {};
        if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;
        

        std::string productName = std::string(extDevInfo.name);


        //- check if device has multizone capabilities and if its currently using them
        bool currMulti = false;
        int numZones = 0;
        if (extDevInfo.multizone || extDevInfo.extended_multizone) {
            
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
        std::cout << "  Device Name: " << extDevInfo.name << std::endl;
        std::cout << "  Device Vecndor ID: " << extDevInfo.vid << std::endl;
        std::cout << "  Device Product ID: " << extDevInfo.pid << std::endl;
        std::cout << "  Device Firmware Major: " << extDevInfo.firmwareMajor << std::endl;
        std::cout << "  Device Firmware Minor: " << extDevInfo.firmwareMinor << std::endl;
    



        std::cout << "\n\n----- Device Features" << std::endl;
        if(extDevInfo.hev) std::cout << "  HEV: True" << std::endl;
        if(extDevInfo.color) std::cout << "  Color: True" << std::endl;
        if(extDevInfo.chain) std::cout << "  Chain: True" << std::endl;
        if(extDevInfo.matrix) std::cout << "  Matrix: True" << std::endl;
        if(extDevInfo.relays) std::cout << "  Relays: True" << std::endl;
        if(extDevInfo.buttons) std::cout << "  Buttons: True" << std::endl;
        if(extDevInfo.infrared) std::cout << "  Infrared: True" << std::endl;
        if(extDevInfo.multizone) std::cout << "  Multizone: True" << std::endl;
        if(extDevInfo.extended_multizone) std::cout << "  Extended Multizone: True" << std::endl;
        if(extDevInfo.tempLowRange.has_value()) std::cout << "  Minimum Kelvin: " << extDevInfo.tempLowRange.value() << std::endl;
        if(extDevInfo.tempUpRange.has_value()) std::cout << "  Maximum Kelvin: " << extDevInfo.tempUpRange.value() << std::endl;


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

    for (size_t i = 0; i < addrs.size(); i++) {

        header.protocol = 1024;
        header.addressable = 1;
        header.source = SOURCE;
        header.sequence = 0;
        header.size = HEADER_SIZE; 
        header.pkt_type = GET_SERVICE;
        device.address = addrs[i];                        
        device.port = htons(DEFAULT_LIFX_PORT);


        //- get info about device and check if it supports colros
        extendedDeviceInfo extDevInfo = {};
        if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;
        

        if (!extDevInfo.color) {
            std::cerr << "Selected device " << inet_ntoa(device.address) << " does not support colors" << std::endl;
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
        header.sequence++;
    
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




bool setColorZonesF(uint32_t ip, uint32_t duration, bool random, std::vector<HSBK*>& hsbkVec) {


    if (hsbkVec.empty()) {
        std::cerr << "Colors vector empty" << std::endl;
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
    extendedDeviceInfo extDevInfo = {};
    if (!getExtendedDeviceInfo(&header, &device, extDevInfo)) return false;
    


    //- get number of zones depending on if extended or normal multizone
    int numZones = 0;


    if (extDevInfo.extended_multizone) {


        //- send getExtendedColorZones, recieve stateExtendedColorZones - to get number of zones 
        header.pkt_type = GET_EXTENDED_COLOR_ZONES;
        Response* extColZonResp = sendPacket<nullPtr>(&header, nullptr, &device, {STATE_EXTENDED_COLOR_ZONES});                                              
        if (extColZonResp == nullptr) {
            std::cout << "No response from device to getExtendedColorZones - (setColorZonesF)" << std::endl;
            return false;
        } 
        stateExtendedColorZones* sECZ = const_cast<stateExtendedColorZones*>(reinterpret_cast<const stateExtendedColorZones*>(extColZonResp->content));
        header.sequence++;
        numZones = sECZ->zones_count;
        delete(extColZonResp->content);
        delete(extColZonResp);



    } else if (extDevInfo.multizone) {


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
        

        bool setColResult = setMultiZoneLight(&header, &device, duration, finalVec, extDevInfo.extended_multizone);
        if (!setColResult) {
            return false;
        }

    } else {

        if (static_cast<int>(hsbkVec.size()) != numZones) {
            std::cerr << "Number of colors provided does not match number of zones of light" << std::endl;
            std::cerr << "Use random if you do not want to display exact colors" << std::endl;
            return false;
        }


        bool setColResult = setMultiZoneLight(&header, &device, duration, hsbkVec, extDevInfo.extended_multizone);
        if (!setColResult) {
            return false;
        }
    }

    for (HSBK* hsbk : hsbkVec) {
        delete(hsbk);
    }
    delete(&hsbkVec);
    return true;


}





























































