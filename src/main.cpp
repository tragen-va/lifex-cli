#include <stdint.h>          // for uint16_t, uint32_t
#include <iostream>          // for basic_ostream, operator<<, cerr, endl
#include <string>            // for char_traits, basic_string, string
#include "helperFuncts.h"    // for listScenes, parseArgs, printHelp
#include "lifxFuncts.h"      // for setPower, setColorZonesF, listDevices
#include "structsAndDefs.h"  // for inputArgs
uint32_t SOURCE;






int main(int argc, char** argv) {


    //- parse args and fill out inputAtgs struct
    inputArgs* options = new inputArgs();              
    if (!parseArgs(argc, argv, options)) return -1;



    if (options->help) {
        printHelp();
        return 0;
    } 

    else if (options->list) {
        listDevices();
        return 0;
    }

    else if (options->listScene) {
        if (listScenes(PATH_TO_SCENES)) return 0;
        return -1;
        
    }



    if (options->all && options->ip) {
        std::cerr << "Cannot select all and specific ip at once" << std::endl;
        std::cerr << "Use -h for usage information" << std::endl;
        return -1;
    } else if (!options->all && !options->ip) {
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
    

    else if (options->loadScene) {
        if (options->all) {
            std::cerr << "loadScene cannot be performed on --all" << std::endl;
            std::cerr << "Use -h for usage information" << std::endl;
            return -1;
        }
        if (loadScene(options->address, options->oldScene, options->durVal)) return 0;
        return -1;
    }
    
    else if (options->saveScene) {
        
        if (options->all) {
            std::cerr << "saveScene cannot be performed on --all" << std::endl;
            std::cerr << "Use -h for usage information" << std::endl;
            return -1;
        }
        if (saveScene(options->address, options->newScene)) return 0;
        return -1;
    }



    return 0;    
}





















