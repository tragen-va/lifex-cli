// helperuncts.cpp



#include "helperFuncts.h"
#include <arpa/inet.h>       // for inet_aton
#include <errno.h>           // for errno, ERANGE
#include <getopt.h>          // for required_argument, no_argument, getopt_long
#include <math.h>            // for fmax, fmin
#include <netinet/in.h>      // for in_addr
#include <unistd.h>          // for optarg, optind
#include <cstdlib>           // for strtoul
#include <exception>         // for exception
#include <filesystem>        // for path, directory_entry, directory_iterator
#include <fstream>           // for basic_ostream, operator<<, endl, basic_i...
#include <iomanip>           // for operator<<, setw, setfill
#include <iostream>          // for cerr, cout
#include <limits>            // for numeric_limits
#include <random>            // for random_device, uniform_int_distribution
#include <sstream>           // for basic_ostringstream
#include <stdexcept>         // for range_error, invalid_argument, out_of_range
#include <tuple>             // for tuple, get, make_tuple
#include <vector>            // for vector
#include "structsAndDefs.h"  // for HSBK, SOURCE, inputArgs





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

    
        hsbk->saturation = (maxVal == 0) ? 0 : (int)((delta / maxVal) * 65535);
        hsbk->brightness = (int)(maxVal * 65535);
        hsbk->kelvin = 3500;             // Default Kelvin (LIFX default: 3500K for colors)
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

    std::cout << R"(
    Usage:
      lifx [OPTIONS] COMMAND [ARGS]

      Target device must be specified, either explicitly with --ip or for all 
      devices on the network with --all. Use --list to view available devices.

    Commands:
      --on, -o                          Turn a light on (can be used with --all)
      --off, -f                         Turn a light off (can be used with --all)
      --setColor {hex}, -c              Set a light to a single color (hex format)
      --setColors {hex,hex}, -s         Set each zone of a multi-zone light to corresponding colors
                                        (number of colors must match number of zones, use --info to check)
      --setColorsR {hex,hex}, -r        Assign input colors randomly to zones
      --brightness {percent}, -b        Set brightness (0-100%)
      --warmth {kelvin}, -w             Set warmth in kelvin (use --info for supported range)
      --loadScene {sceneName}, -v       Display a saved scene on selected light 
                                        (selected scene must have at least as many zones than light)
      --saveScene {sceneName}, -x       Save the color info for each zone to be used later


    Query Commands:
      --list, -l                        List all LIFX devices (type, IP, MAC, status)
      --info, -i                        Show detailed information about a device (supports --all)
      --listScene, -e                   List all saved scenes and the number of zones associated

    Target Selection:
      --ip {ip}, -p                     Specify target device by IPv4 address (dot notation)
                                        (Addresses can be obtained via --list)
      --all, -a                         Apply command to all LIFX devices on the network
                                        (Not available for all commands)
      --duration {milliseconds}, -d     Set transition duration for actions (in milliseconds)
                                        (Not available for all commands)

    Color Format:
      Colors must be provided as hex values inside brackets, separated by commas.
      Use **0x** notation, not **#**.

      Examples:
        lifx --setColor [0x2e6f40] --ip 192.168.8.141
        lifx --setColorsR [0x2e6f40,0x228b22,0xe29627,0x89cef9,0x485d18,0x290c22,0xef7ec0] --ip 192.168.8.141

    )" << std::endl;
}




/// @brief retuns all the .json files in a perticular directory as well as the number of zones for each
/// @param string of the path to the directory the scenes are stored
/// @return vector of tuples of string and ints all json files in directory and their number of zones, or empty if none 
///         std::get<get>vector[0] = "error" if error
std::vector<std::tuple<std::string, int>> getScenes(const std::string& path) {

    std::vector<std::tuple<std::string, int>> scenes;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                //- check each file to make sure it is formatted correctly and get numZones
                try {
                    std::ifstream file(entry.path().string());
                    if (!file.is_open()) {
                        std::string error = "error";
                        std::tuple<std::string, int> errorTup = std::make_tuple(error, 0);
                        scenes.push_back(errorTup);
                        std::cerr << "[-] Error: Unable to open file: " << entry.path().string() << std::endl;
                        return scenes;
                    }

                    nlohmann::json sceneJson;
                    file >> sceneJson; // Parse the JSON file
                    file.close();

                    // Validate JSON format
                    if (!sceneJson.is_array() || sceneJson.empty() || 
                        !sceneJson[0].contains("scene") || !sceneJson[0]["scene"].is_object() ||
                        !sceneJson[0]["scene"].contains("sceneName") || !sceneJson[0]["scene"]["sceneName"].is_string() ||
                        !sceneJson[0]["scene"].contains("zoneCount") || !sceneJson[0]["scene"]["zoneCount"].is_number_integer() ||
                        !sceneJson[0]["scene"].contains("zones") || !sceneJson[0]["scene"]["zones"].is_array()) {
                        
                        std::cerr << "[-] Error: Invalid JSON format in " << entry.path().string() << std::endl;
                        continue;
                    }

                    // Extract zoneCount
                    int zoneCount = sceneJson[0]["scene"]["zoneCount"];
                    std::tuple<std::string, int> scene = std::make_tuple(entry.path().filename().string(), zoneCount);
                    scenes.push_back(scene);
                    continue;

                } catch (const nlohmann::json::parse_error& e) {
                    std::cerr << "[-] JSON Parsing error in " << entry.path().string() << ": " << e.what() << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[-] Error processing file " << entry.path().string() << ": " << e.what() << std::endl;
                }
            }
        }
        if (scenes.empty()) {
            return {};
        }
    } catch (const std::filesystem::filesystem_error& e){
        std::string error = "error";
        std::tuple<std::string, int> errorTup = std::make_tuple(error, 0);
        scenes.push_back(errorTup);
        std::cerr << "[-] Cannot access directory at " << path << ": " << e.what() << std::endl;
        return scenes;
    }
    return scenes;

}






/// @breif print out all the vaid saved scenes as well as the number of zoens associated 
/// @param the path to the scenes directory as a std::string
/// @return bool result
bool listScenes(const std::string& path) {


    //- get all scens and their numZones
    std::vector<std::tuple<std::string, int>> scenes = getScenes(path);
    if (scenes.empty()) {
        std::cerr << "You have no saved scenes at " << path << std::endl;
        return false;
    } else if (std::get<0>(scenes[0]) == "error" && std::get<1>(scenes[0]) == 0) {
        return false;
    }
    

    //- print scenes
    bool first = true;
    int sceneNameWidth = 26;
    int numZonesWidth = 18;

    for (size_t i = 0; i < scenes.size(); i++) {

        if (first) {
            std::cout << R"(
--------------------------------------------------- 
| Scene Name                | Number of Zones     | 
---------------------------------------------------
            )" << std::endl;
        
            first = false;
        } 
        
        //- strip off .json from scene names
        std::string sceneName = std::get<0>(scenes[i]).substr(0, std::get<0>(scenes[i]).size() - 5);


    std::cout << "* " << std::setw(sceneNameWidth) << std::left << sceneName << " | " << std::setw(numZonesWidth) << std::left << std::get<1>(scenes[i]) << " | " << std::endl;

    }

    return true;

}







// verify that any args passed as single dash only have 1 letter
bool valSinleDashArgs(int argc, char** argv) {

    for (int i = 1; i < argc; i++) {
        std::string tempArg = std::string(argv[i]);
        if (tempArg[0] == '-' && tempArg.size() > 2 && tempArg[1] != '-') {
            std::cerr << "[-] Invalid input: " << tempArg << std::endl;
            std::cerr << "    Single dash '-' can only be followed by single charactor" << std::endl;
            return false;
        }
    }

    return true;
}







// --listScene -e
// --loadScene -v {sceneName}
// --saveScene -x {newSceneName}

// getScenes    fucntion that goes into scenes directory and lists the names of each scene and num zones. Also used by saveScene and loadScene to make sure no duplicates
// listScenes   prints out list from getScenes


bool parseArgs(int argc, char** argv, inputArgs* options) {

    

    const char* const shortOpts = "hofs:lib:w:c:r:p:ad:ev:x:";
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
    {"listScene", no_argument, nullptr, 'e'},
    {"loadScene", required_argument, nullptr, 'v'},
    {"saveScene", required_argument, nullptr, 'x'},
    {0, 0, 0, 0}

    };




    if (!valSinleDashArgs(argc, argv)) return false;


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

        case 'e': // listScene
            options->listScene = true;
            break;
        case 'v': // loadScene   
            if (optarg) { 
                options->oldScene = std::string(optarg);
                options->loadScene = true;
            } else {
                std::cerr << "[-] Missing argument for -v / --loadScene" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
            break;
        case 'x': // saveScene   
            if (optarg) { 
                options->newScene = std::string(optarg);
                options->saveScene = true;
            } else {
                std::cerr << "[-] Missing argument for -x / --saveScene" << std::endl;
                std::cerr << "\nUse -h for usage information" << std::endl;
                return false;
            }
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














































