#include <stdint.h>
#include <iostream>
#include <cstdlib>
#include <cstring> //strlen()  -  memcpy()

#pragma pack(push, 1)
typedef struct lx_frame_t {
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


#define StateService 2







class LxPacket {
public:



    lx_frame_t* lxFrame;
    lx_frame_t* receivedFrame;
    char* response;
    stateService* sService;
    uint16_t payloadSize;




    LxPacket(){
        lxFrame = (lx_frame_t *)malloc(sizeof(struct lx_frame_t));
        memset(&lxFrame, 0, sizeof(lx_frame_t));
        lxFrame->protocol = 1024;
        lxFrame->addressable = 1;
        lxFrame->reserved_1 = 0;  //origin
        lxFrame->sequence = 0;
        lxFrame->source = std::rand() % 257;
    }
    

    LxPacket(uint16_t Pkt_type){
        lxFrame = (lx_frame_t *)malloc(sizeof(struct lx_frame_t));
        lxFrame->protocol = 1024;
        lxFrame->addressable = 1;
        lxFrame->reserved_1 = 0;  //origin
        lxFrame->sequence = 0;
        lxFrame->source = std::rand() % 257;
        lxFrame->pkt_type = Pkt_type;
    }






    void setAddressable(bool x) {
        if(x)
            lxFrame->addressable = 1;
        else
            lxFrame->addressable = 0;
    }
    void setTagged(bool x) {
        if(x)
            lxFrame->tagged = 1;
        else
            lxFrame->tagged = 0;
    }
    void setRes_required(bool x) {
        if(x)
            lxFrame->res_required = 1;
        else
            lxFrame->res_required = 0;
    }   

    void setAck_required(bool x) {
        if(x)
            lxFrame->ack_required = 1;
        else
            lxFrame->ack_required = 0;
    }
    void incrementSequence() {
        if (lxFrame->sequence == 254)            // can probably make beter
            lxFrame->sequence = 0;
        else
            lxFrame->sequence++;
    }
    void setSize() {
        lxFrame->size = 36 + payloadSize;
    }
    void setPayloadSize(int x) {
        payloadSize = x;
    }
    void extractTarget() {

        // add logic
        // not sure if need or can just take from decoded packet
        // only for printing I think
    }

    bool unpack(char *hex) {

        int str_len = strlen(hex);
        
        if (str_len < 72) {
          return false;
        }

        uint8_t buffer[36] = {};
        for (int i = 0; i < (str_len / 2) && i < 72; i++) {
          unsigned int nxt;
          sscanf(hex + 2 * i, "%02x", &nxt);
          buffer[i] = (uint8_t)nxt;
        }
        memcpy(receivedFrame, buffer, 36);

        // get response
    
        if (str_len > 72) {
            response = (char*)malloc((str_len - 72));
            processResponse(receivedFrame->pkt_type);            

        }
    }


    // posisbly get rid of pk_type and ljust look at reiced
    bool processResponse(uint16_t pk_type) {

            if(pk_type == StateService) { 
                int str_len = strlen(response);
                if (str_len != 5) {
                    return false;
                }
            
                uint8_t buffer[str_len / 2] = {};
                for (int i = 0; i < (str_len / 2); i++) {
                    unsigned int nxt;
                    sscanf(response + 2 * i, "%02x", &nxt);
                    buffer[i] = (uint8_t)nxt;    
                }

                memcpy(response, buffer, (str_len / 2));

            }
    }




    char* pack() {
        
        char* data = reinterpret_cast<char*>(&lxFrame);        
        size_t length = sizeof(lx_frame_t);

        std::string hexStr;
        char buffer[3];
        for (size_t i = 0; i < length; i++) {
            snprintf(buffer, sizeof(buffer), "%02X", static_cast<unsigned char>(data[i]));
            hexStr.append(buffer);

        }

        return hexStr.c_str();



    }






};


















// lifeX {-on(all), -off(all), -setColor( color )} 
int main(int argc, char* argv[]) {

    

    // create socket
    struct servAddr_in sockAddr = {0};
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("failed to create socket");
        exit(EXIT_FAILURE);
    }
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(56700);
    servAddr.sin_addr.s_addr = INADDR_ANY;  // 0.0.0.0





    // parse command params
    if (argc == 1) {
        std::cout << "No commands provided --- exiting" std::endl;
        return 1;
    }


    for (int i = 1; i < argc; i++) {


        if (strcmp(argv[i]), "-on") {

            LxPacket curr = new LxPacket(StateService);
            curr->setTagged(true);
            size_t len = sendto(sockfd, (const char*)curr->pack(), strlen(curr->pack(), 0, (struct sockaddr*)&servAddr, sizeof(serverAddr)));

        }
        else if (strcmp(argv[i]), "-off") {




        }
        else if (strcmp(argv[i]), "-setColor") {


        }
        





    }


    

    return 0;


}




