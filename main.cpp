#include <iostream>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <ifaddrs.h>







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
typedef struct setPower {
    uint16_t level;         // The power level can be either standby (0) or enabled (65535)
    uint32_t duration;      // The duration is the power level transition time in milliseconds
} setPower;   
#pragma pack(pop)




#pragma pack(push, 1)
typedef struct lxDevice {
    uint8_t service;
    uint32_t port;
    uint64_t target;
} lxDevice;
#pragma pack(pop)









#define GetService 2
#define LIFX_PORT 56700
#define BUFFER_SIZE 1024
#define SetPower 117




class LxPacket {

public:

    lx_frame_t* lxFrame;



    LxPacket() {

        
        lxFrame = (lx_frame_t *)malloc(sizeof(lx_frame_t));
        memset(lxFrame, 0, sizeof(lx_frame_t));
    }

};






int main(int argc, char* argv[]) {





    if (argc == 1) { 
        std::cout << "No paramiters passed" << std::endl;
        return -1;
    }

    if (strcmp(argv[1], "-list") == 0) {

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



        // bind socket to recieve response
        struct sockaddr_in localAddr;
        memset(&localAddr, 0, sizeof(localAddr));
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(LIFX_PORT);
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);


        if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            perror("Bind failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }


        // Set up the broadcast address structure
        struct sockaddr_in broadcastAddr;
        memset(&broadcastAddr, 0, sizeof(broadcastAddr));
        broadcastAddr.sin_family = AF_INET;
        broadcastAddr.sin_port = htons(LIFX_PORT);
        broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);



        LxPacket* curr = new LxPacket();
        curr->lxFrame->size = sizeof(curr->lxFrame);
        curr->lxFrame->protocol = 1024;
        curr->lxFrame->addressable = 1;
        curr->lxFrame->tagged = 1;
        curr->lxFrame->source = 123;
        curr->lxFrame->pkt_type = GetService;
        


        // send boradcast getService message
        ssize_t sent_bytes = sendto(sockfd, curr->lxFrame, sizeof(*curr->lxFrame), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
        if (sent_bytes < 0) {
            perror("sendto failed");
            close(sockfd);
            delete curr;
            return -1;
        } else {
            std::cout << "Broadcasted packet to network" << std::endl;
        }




        // Receive responses
        char buffer[BUFFER_SIZE];
        struct sockaddr_in senderAddr;
        socklen_t senderAddrLen = sizeof(senderAddr);

        std::cout << "Waiting for responses..." << std::endl;


        // Set socket to non-blocking so we can stop listening after some time
        struct timeval tv;
        tv.tv_sec = 5;  // Wait for 5 seconds
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                
        std::vector<in_addr> devices;
        while (true) {
            ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                          (struct sockaddr*)&senderAddr, &senderAddrLen);

            if (recv_bytes < 0) {
                // Timeout or no more data, break the loop
                break;
            }

            if (recv_bytes < 36) { // minimum header size with no response payload      
                std::cout << "[-] Invalid response from device - (too small)" << std::endl;
            }


        // check if response is lifx protocol
        lx_frame_t* response = (lx_frame_t*)buffer;
        bool include = true;





            if((response->size != recv_bytes || response->target != 0) && response->pkt_type == 3) {
                
                    for (int i = 0; i < devices.size(); i++) {
                        if (senderAddr.sin_addr.s_addr == devices[i].s_addr ) {
                            include = false;   
                                
                        }
                    }
                    if (include)
                        devices.push_back(senderAddr.sin_addr);
            }




 
        }
        std::cout << "******   Devices Found on Your network   ******" << std::endl;
        if (devices.size() == 0) 
            std::cout << "No Devices Found" << std::endl;

        for (int i = 0; i < devices.size(); i++) {
            std::cout << "Device " << (i + 1) << ": " << inet_ntoa(devices[i]) << std::endl;
        }

        // Close the socket
        close(sockfd);
    














































    } else if (strcmp(argv[1], "-select") == 0) {



        if (argc < 2) {
            std::cout << "[-] Missing paramiter - (device)" << std::endl;
            return -1;    
        }
  
        if (strcmp(argv[2], "all") == 0) {
            
            
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



            // bind socket to recieve response
            struct sockaddr_in localAddr;
            memset(&localAddr, 0, sizeof(localAddr));
            localAddr.sin_family = AF_INET;
            localAddr.sin_port = htons(LIFX_PORT);
            localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
                perror("Bind failed");
                close(sockfd);
                exit(EXIT_FAILURE);
            }


            // Set up the broadcast address structure
            struct sockaddr_in broadcastAddr;
            memset(&broadcastAddr, 0, sizeof(broadcastAddr));
            broadcastAddr.sin_family = AF_INET;
            broadcastAddr.sin_port = htons(LIFX_PORT);
            broadcastAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);



            LxPacket* curr = new LxPacket();
            curr->lxFrame->size = sizeof(curr->lxFrame);
            curr->lxFrame->protocol = 1024;
            curr->lxFrame->addressable = 1;
            curr->lxFrame->tagged = 1;
            curr->lxFrame->source = 123;
            curr->lxFrame->pkt_type = GetService;
            


            // send boradcast getService message
            ssize_t sent_bytes = sendto(sockfd, curr->lxFrame, sizeof(*curr->lxFrame), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
            if (sent_bytes < 0) {
                perror("sendto failed");
                close(sockfd);
                delete curr;
                return -1;
            } else {
                std::cout << "Broadcasted packet to network" << std::endl;
            }

            // Receive responses
            char buffer[BUFFER_SIZE];
            struct sockaddr_in senderAddr;
            socklen_t senderAddrLen = sizeof(senderAddr);

            std::cout << "Waiting for responses..." << std::endl;


            // Set socket to non-blocking so we can stop listening after some time
            struct timeval tv;
            tv.tv_sec = 5;  // Wait for 5 seconds
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                    
            

            std::vector<lxDevice*> devices;

            
             while (true) {
                ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                              (struct sockaddr*)&senderAddr, &senderAddrLen);

                if (recv_bytes < 0) {
                    // Timeout or no more data, break the loop
                    break;
                }


                
                if (recv_bytes < 36) { // minimum header size with no response payload      
                    std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                }




                lx_frame_t* response = (lx_frame_t*)buffer;
                if((response->size != recv_bytes || response->target != 0) && response->pkt_type == 3) {
                    bool alreadyIn = false;
                    for (int i = 0; i < devices.size(); i++) {
                        if (response->target == devices[i]->target)
                            alreadyIn = true;
                    }
                    
                    if(!alreadyIn) {
                        lxDevice* device = new lxDevice();
                        stateService* ss = reinterpret_cast<stateService*>(&buffer[recv_bytes - 5]);
                        device->service = ss->service;
                        device->port = ss->port;
                        device->target = response->target;
                        devices.push_back(device);

                    }
                  } 
            }

            std::cout << "Number of devices conected: " << devices.size() << std::endl;            

















































        } else { // assume argv[2] is ip
        
        
            // check if argv is valid ip
            in_addr* deviceIp;
            int result = inet_aton(argv[2], deviceIp);
            if (!result) {
                std::cout << "[-] IP address invalid" << std::endl;
                return -1;
            }
    



            // create socket 
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd == -1) {
                perror("failed to create socket");
                exit(EXIT_FAILURE);
            }




    
            // bind socket to recieve response
            struct sockaddr_in localAddr;
            memset(&localAddr, 0, sizeof(localAddr));
            localAddr.sin_family = AF_INET;
            localAddr.sin_port = htons(LIFX_PORT);
            localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            


            if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
                perror("Bind failed");
                close(sockfd);
                exit(EXIT_FAILURE);
            }


            localAddr.sin_addr.s_addr = deviceIp->s_addr;







            // Set up the structure for GetService to confirm port
            
            LxPacket* curr = new LxPacket();
            curr->lxFrame->size = sizeof(curr->lxFrame);
            curr->lxFrame->protocol = 1024;
            curr->lxFrame->addressable = 1;
            curr->lxFrame->source = 123;
            curr->lxFrame->pkt_type = GetService;




            // send  getService message
            ssize_t sent_bytes = sendto(sockfd, curr->lxFrame, sizeof(*curr->lxFrame), 0, (struct sockaddr*)&localAddr, sizeof(localAddr));
            if (sent_bytes < 0) {
                perror("sendto failed");
                close(sockfd);
                delete curr;
                return -1;
            } else {
                std::cout << "Atempting to reach " << argv[2] << std::endl;
            }





             // Receive responses
            char buffer[BUFFER_SIZE];
            struct sockaddr_in senderAddr;
            socklen_t senderAddrLen = sizeof(senderAddr);

            std::cout << "Waiting for responses..." << std::endl;


            // Set socket to non-blocking so we can stop listening after some time
            struct timeval tv;
            tv.tv_sec = 5;  // Wait for 5 seconds
            tv.tv_usec = 0;
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                    
            lxDevice* device;
            while (true) {
                ssize_t recv_bytes = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                              (struct sockaddr*)&senderAddr, &senderAddrLen);

                if (recv_bytes < 0) {
                    // Timeout or no more data, break the loop
                    break;
                }

                 
                if (recv_bytes < 36) { // minimum header size with no response payload      
                    std::cout << "[-] Invalid response from device - (too small)" << std::endl;
                }

            

            // check if response is lifx protocol
            lx_frame_t* response = (lx_frame_t*)buffer;
                if (response->target != 0) {
                    if (response->pkt_type == 3)   
                            

                        if (recv_bytes != 41) {
                            std::cout << "[-] Inproper response from device - (not expected size recived)" << std::endl;
                            return -1;
                        }
                        stateService* ss = reinterpret_cast<stateService*>(&buffer[recv_bytes - 4]);
                        device->service = ss->service;
                        device->port = ss->port;
                        device->target = response->target;
                        break;      // only need 1 response from getService
                        
                }

            




                            


            }



                std::cout << "recieved response from " << device->target << std::endl; 
                // make sure port is correct












        }


    
    }





    return 0;





}

