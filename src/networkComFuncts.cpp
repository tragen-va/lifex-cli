// networkComFuncts.cpp



#include <arpa/inet.h>         // for htonl, htons
#include <errno.h>             // for errno, EAGAIN, EWOULDBLOCK
#include <netinet/in.h>        // for in_addr, sockaddr_in, INADDR_ANY, INAD...
#include <stdio.h>             // for perror
#include <sys/socket.h>        // for setsockopt, AF_INET, SOL_SOCKET, bind
#include <sys/time.h>          // for timeval
#include <sys/types.h>         // for ssize_t
#include <unistd.h>            // for close
#include <cstdlib>             // for exit, EXIT_FAILURE
#include <cstring>             // for memset
#include <iostream>            // for basic_ostream, char_traits, operator<<
#include <vector>              // for vector
#include "networkComFuncts.h"  // for getAllDevices
#include "structsAndDefs.h"    // for lx_frame_t, BUFFER_SIZE, DEFAULT_LIFX_...






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
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    perror("recvfrom failed unexpectedly");
                    break;
                }
            }



            if (recv_bytes < static_cast<int>(sizeof(lx_frame_t))) {
                std::cerr << "[-] Response too small to be a valid lx_frame_t" << std::endl;
                continue;
            }


            bool included = false;
            lx_frame_t* response = (lx_frame_t*)buffer;
            std::cout << std::flush;
            if (response->target != 0 && response->source == SOURCE && (response->reserved_1 != 0 || response->reserved_3 != 0 || response->reserved_4 != 0 || response->reserved_5 != 0)) {

                if (devAddrs.size() == 0) {
                
                    devAddrs.push_back(senderAddr.sin_addr);
                    continue;
                } 
                for (size_t i = 0; i < devAddrs.size(); i++) {
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






























































