#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <chrono>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "clock_utils.hpp"

using namespace std;
//
//***********************************************************************************************
//
// Class clock_client: Multicast listening client for clock synchronization. Uses GLIBC.
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
class clock_client {

        // Declare multicast port
        const short multicast_port {5000};

        // Declare multicast address
        const string cstrMulticastAddress {"238.10.50.50"};

        // Declare client id 
        uint32_t client_id;

        // Declare clock id 
        uint32_t clock_id {0};

        // Declare file descriptor for socket i/o
        int sd;

        // Declare multicaster source ip address
        struct sockaddr stMulticasterSourceIP;
        int nLen {sizeof(struct sockaddr)};

        // Declare data buffer for transmission
        enum { max_length = 256 };
        char data_[max_length];

public:

        // Constructor
        clock_client (uint32_t pClientid, uint32_t pClockid) : client_id(pClientid), clock_id(pClockid) 
        {
        }

        // Join multicast group and Start receiving messages
        void StartReceiving ()
        {
             // Start Receiving by listening to multicast
             StartReceiving_impl();

             // Declare number of bytes received 
             int msg_num {0};
             int bytes_recvd {0};

             // Read incoming multicast data
             while((bytes_recvd = recvfrom (sd, data_, sizeof(ClockSyncMessage), 0, &stMulticasterSourceIP, (socklen_t*) &nLen)) > -1)
             {
                // Process Received data in its handler
                ReceiveHandler(bytes_recvd);

                // Reset reception buffer
                memset(data_,0,max_length);
             }
        }

private:

        // Start Receiving multicast implementation
        void StartReceiving_impl()
        {
             // Declare the receiving socket and group
             struct sockaddr_in localSock;
             struct ip_mreq group;

             // Create a datagram socket on which to receive 
             sd = socket(AF_INET, SOCK_DGRAM, 0);

             // Check it is a valid descriptor
             if (sd < 0)
             {
                 cerr << "Error Opening datagram client socket" << endl;
                 exit(EXIT_FAILURE);
             }

             // Set option SO_REUSEADDR to allow multiple instances of this client 
             int reuse = 1;
             if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const void *)(&reuse), sizeof(reuse)) < 0)
             {
                 cerr << "Error setsockopt Setting SO_REUSEADDR" << endl;
                 close(sd);
                 exit(EXIT_FAILURE);
             }

             // Set option SO_REUSEPORT 
             reuse = 1;
             if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (const void *)(&reuse), sizeof(reuse)) < 0)
             {
                 cerr << "Error setsockopt Setting SO_REUSEPORT" << endl;
                 close(sd);
                 exit(EXIT_FAILURE);
             }

             // Bind to the proper port number with the IP address
             memset((char *) &localSock, 0, sizeof(localSock));
             localSock.sin_family = AF_INET;
             localSock.sin_port = htons(multicast_port);
             localSock.sin_addr.s_addr = INADDR_ANY;
             if (bind(sd, (struct sockaddr*)&localSock, sizeof(localSock)))
             {
                 cerr << "Error Binding datagram socket [" << client_id << "]" << endl;
                 close(sd);
                 exit(EXIT_FAILURE);
             }

             // Join the multicast group
             group.imr_multiaddr.s_addr = inet_addr(cstrMulticastAddress.c_str());
             group.imr_interface.s_addr = INADDR_ANY;
             if (setsockopt(sd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
             {
                 cerr << "Error setsockopt joining multicast group" << endl;
                 close(sd);
                 exit(EXIT_FAILURE);
             }
        }

       // Event Handler for receiving multicast messages
       void ReceiveHandler (int bytes_recvd)
       {
         try 
         {
                // Get the immediate (client) time stamp when server message is received 
                uint64_t TimeStamp = GetCurrentTimeSinceEpoch();
                  
                // Check if message is fully received
                if (bytes_recvd == sizeof(ClockSyncMessage)) 
                {
                    // Get a pointer to the received sync message
                    ClockSyncMessage* oReceivedMessage = reinterpret_cast<ClockSyncMessage*>(data_);

#if !defined NO_PRINT
		    // Indicate Received Message
		    PrintSyncMessage(to_string(client_id) + "-recvd", *oReceivedMessage);
#endif
                    // Validate the message's checksum before processing 
                    // The previous message size check and current checksum validation ensures message integrity
                    if (ValidateCheckSum (*oReceivedMessage)) 
                    {
                        // Check if a specific server is only to be processed
                        if (clock_id == 0 || clock_id == oReceivedMessage->clock_id) 
                        {
                            // Set the timestamp unto the client time field
                            oReceivedMessage->client_ts = TimeStamp;

                            // Process the (client-time-stamped) received message
                            ProcessReceivedMessage (oReceivedMessage);
                        }
                    }
                }
         }
         catch (exception& e)
         {
              cerr << e.what() << endl;
         }
      }
 
      // Process Received Message 
      void ProcessReceivedMessage (ClockSyncMessage* poMsg) 
      {
           // Declare response message
           ClockSyncMessage oResponseMsg;

           // Set up the response message fields
           oResponseMsg.clock_id  = client_id;
           oResponseMsg.server_ts = poMsg->server_ts;
           oResponseMsg.client_ts = poMsg->client_ts;
           oResponseMsg.checksum  = ComputeCheckSum(oResponseMsg);

           // Send response message
           SendMessage(oResponseMsg);
      }

      // Send Message to multicast server
      bool SendMessage(ClockSyncMessage &poMsg)
      {
           // Check the validity of the descriptor
           if (sd > -1) 
           {
               // Set up the response buffer
               int datalen = sizeof(ClockSyncMessage);
               char* dataptr_= reinterpret_cast<char*>(&poMsg);

               // Send client unicast response to multicast server 
               if (sendto(sd, dataptr_, datalen , 0, (struct sockaddr*)&stMulticasterSourceIP, nLen) < 0) 
               {
                   cerr << "Error in unicast sendto from client";
                   exit(EXIT_FAILURE);
               }

#if !defined NO_PRINT
	       // Indicate Reply Message
      	       PrintSyncMessage("sentm---", poMsg);
#endif
               // Successful reply to server
               return true;
          }

          // Unsuccessful send
          return false;
      }
};

// *****************************
// Main entry point
// *****************************
int main (int argc, char* argv[])
{
  try
  {
       // Check for the required input
       if (argc < 2)
       {
          cerr << "Usage: clock_client <client_id> [clock_id]";
          return 1;
       }

       // Declare and read the required client_id 
       uint32_t clock_id {0};
       uint32_t client_id = atoi(argv[1]);

       // Check if the optional clock_id has been specified
       if (argc > 2)
       {
           clock_id = atoi(argv[2]);
       }

       // Declare the client clock
       clock_client clock (client_id, clock_id);

       // Start client receiving
       clock.StartReceiving ();
  }
  catch (exception& e)
  {
       cerr << e.what() << endl;
  }

  return 0;
}

