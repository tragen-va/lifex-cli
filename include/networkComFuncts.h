#ifndef NETWORK_COM_FUNCTS_H
#define NETWORK_COM_FUNCTS_H

// any function that sends or recieves packets directly



#include <initializer_list>  // for initializer_list
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
#include <algorithm>           // for std::find()




/// @brief sends lifx message useing udp socket
/// @tparam T is struct coresponding to content of message, use nullPtr struct if none
/// @param header struct conting information about lifx packet
/// @param optinal if message has a body
/// @param device struct containing ip and port of target device
/// @param responseTypes is initializer list of all acceptable lifx responseTypes
/// @return if response returns content in response struct, if not returns nullptr
template<typename T>
Response* sendPacket(lx_frame_t* header, T* payload, lxDevice* device, std::initializer_list<int> responseTypes) {


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
        localAddr.sin_port = device->port;
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
        clientAddr.sin_port = device->port;
        clientAddr.sin_addr.s_addr = device->address.s_addr;


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



                lx_frame_t* response = (lx_frame_t*)buffer;
                std::cout << std::flush;
                //std::cout << "Response type: " << response->pkt_type << std::endl; //////////////////////////////////
                if (response->sequence == header->sequence && (std::find(responseTypes.begin(), responseTypes.end(), static_cast<int>(response->pkt_type)) != responseTypes.end())) {

                        device->address = senderAddr.sin_addr;
                        device->target = response->target;
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
std::vector<Response*> sendMultiPacket(lx_frame_t* header, T* payload, lxDevice* device, std::initializer_list<int> responseTypes, int numResponses) {


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
        localAddr.sin_port = device->port;
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
        clientAddr.sin_port = device->port;
        clientAddr.sin_addr.s_addr = device->address.s_addr;


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



                lx_frame_t* response = (lx_frame_t*)buffer;
                std::cout << std::flush;
                //std::cout << "Response type: " << response->pkt_type << std::endl; ////////////////
                if (response->sequence == header->sequence && (std::find(responseTypes.begin(), responseTypes.end(), static_cast<int>(response->pkt_type)) != responseTypes.end())) {
  
                        Response* resp = new Response();
                        device->address = senderAddr.sin_addr;
                        device->target = response->target;
                        resp->size = recv_bytes - HEADER_SIZE;
                        resp->content = new char[resp->size];
                        resp->message_type = response->pkt_type;
                        memcpy((void*)resp->content, buffer + HEADER_SIZE, resp->size); 
                        responses.push_back(resp);
                        respsSoFar++;
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



std::vector<in_addr> getAllDevices();



#endif //NETWORK_COM_FUNCTS_H
