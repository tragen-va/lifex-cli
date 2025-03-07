#ifndef HELPER_FUNCTS_H
#define HELPER_FUNCTS_H


// any function that does not diectly or inderectly interact with lifx devices




#include <stdint.h>          // for uint16_t, uint32_t, uint64_t
#include <string>            // for string
#include <tuple>             // for tuple
#include <vector>            // for vector
#include "json.hpp"          // for json
#include "structsAndDefs.h"  // for HSBK, inputArgs



std::string uint64ToMac(uint64_t target);


void genSource();


bool initScene(nlohmann::json& scene, const std::string& scenePath, int numZones);


uint16_t strToUint16(const char* str);


uint32_t charToUint32(const char* str);


std::vector<HSBK*>* strToHSBKVec(const char* colorInput);


void printHelp();


std::vector<std::tuple<std::string, int>> getScenes(const std::string& path);


bool listScenes(const std::string& path);


bool valSinleDashArgs(int argc, char** argv);


bool parseArgs(int argc, char** argv, inputArgs* options);








#endif //HELPER_FUNCTS_H













































