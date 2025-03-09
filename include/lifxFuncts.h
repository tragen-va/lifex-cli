#ifndef LIFX_FUNCTS_H
#define LIFX_FUNCTS_H


// any function that communicates with lifx deceies through the send packet functions


#include <stdint.h>          // for uint32_t, uint16_t
#include <string>            // for string
#include <vector>            // for vector
#include "structsAndDefs.h"  // for HSBK, lxDevice, lx_frame_t, extendedDevi...



bool listDevices();


bool setPower(bool broadcast, bool brightness, uint32_t ip, uint16_t level, uint32_t duration);


std::vector<HSBK*> getMultiZoneData(lx_frame_t* header, lxDevice* device, bool extended_multizone);


bool setMultiZoneLight(lx_frame_t* header, lxDevice* device, uint32_t duration, std::vector<HSBK*> hsbkVec, bool extended_multizone);


bool getExtendedDeviceInfo(lx_frame_t* header, lxDevice* device, extendedDeviceInfo& extDevInfo);


bool getPathToScenes(std::string& pathToScenes);


bool saveScene(uint32_t ip, std::string sceneName);


bool loadScene(uint32_t ip, std::string sceneName, uint32_t duration);


bool setKelvin(bool broadcast, uint32_t ip, uint32_t duration, uint16_t kelvin);


bool printInfo(bool broadcast, uint32_t ip);


bool setColorF(bool broadcast, uint32_t ip, uint32_t duration, HSBK* hsbk);


bool setColorZonesF(uint32_t ip, uint32_t duration, bool random, std::vector<HSBK*>& hsbkVec);







#endif //LIFX_FUNCTS



































