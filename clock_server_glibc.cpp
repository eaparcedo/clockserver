#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <algorithm>
#include <utility>
#include <numeric>
#include <iomanip>
#include <thread>
#include <cstring>
#include <functional>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
  
#include "clock_utils.hpp"
#include "clock_stats.hpp"
#include "clock_timer.hpp"

using namespace std;
//
//***********************************************************************************************
//
// Class clock_server: Multicast broadcasting server for clock synchronization. Uses GLIBC.
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
class clock_server : public std::enable_shared_from_this<clock_server>{

        // Declare multicast port
        const short multicast_port {5000};

        // Declare multicast address
        const string multicast_address {"238.10.50.50"};

        // Declare clock id 
        uint32_t clock_id;

        // Declare broadcasting period 
        uint32_t interval;

        // Declare message count
        uint64_t message_count {0};

	// Declare the broadcast timer  
	shared_ptr<clock_timer> oBroadcastTimer;

	// Declare the statistics timer  
	shared_ptr<clock_timer> oStatisticsTimer;

        // Declare Outbound Buffer for broadcast messages 
        enum { max_length = 256 };
        char data_[max_length];

        // Declare Inbound Buffer for client responses 
        enum { max_length_recv = 4096 };
        char recv_buffer_[max_length_recv];

        // Declare the statistics processor
        clock_stats stats;

public:

        // Constructor
        clock_server (uint32_t pClockid, uint32_t piInterval) : 

                      clock_id         (pClockid), 
                      interval         (piInterval), 
                      message_count    (0),
                      oBroadcastTimer  (make_shared<clock_timer>()), 
                      oStatisticsTimer (make_shared<clock_timer>()) 
        {
        } 

        // Destructor
        ~clock_server() 
        {
             // Stop the broadcast timer
             if (oBroadcastTimer->is_running()) 
             {
	         oBroadcastTimer->stop();
             }

             // Stop the statistics timer
             if (oStatisticsTimer->is_running()) 
             {
	         oStatisticsTimer->stop();
             }
        }

        // Start broadcasting and receiving reply messages
        void StartBroadcasting ()
        {
             // Start stats timer (every minute)
             oStatisticsTimer->start(60*1000, bind(&clock_server::ProcessStatistics, this));

             // Start broadcast timer (every interval seconds)
             oBroadcastTimer->start(interval*1000, bind(&clock_server::StartBroadcasting_impl, this));

             // Run the service by resting the master thread
             this_thread::sleep_for(chrono::hours(max_length_recv));
        }

private:

       // Event handler for Broadcast timer that performs the multicast
       void StartBroadcasting_impl  ()
       {
             // Build a broadcast message
             ClockSyncMessage oBroadcastMessage = BuildBroadcastMessage();

             // Indicate message has been built
             PrintSyncMessage("BUILT",oBroadcastMessage);

             // Multicast the message
             BroadcastMessage(oBroadcastMessage);
       }

       // Perform the multicast of a built sync message
       void BroadcastMessage(ClockSyncMessage &oBroadcastMessage) 
       {
             // Declare multigroup socket
             struct in_addr localInterface;
             struct sockaddr_in groupSock;
             int sd;

             // Create a datagram socket on which to send 
             sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
             if (sd < 0)
             {
                 cerr << "Error Opening datagram socket";
                 exit(EXIT_FAILURE);
             }

             // Initialize the group sockaddr structure  
             memset((char *) &groupSock, 0, sizeof(groupSock));
             groupSock.sin_family = AF_INET;
             groupSock.sin_addr.s_addr = inet_addr(multicast_address.c_str());
             groupSock.sin_port = htons(multicast_port);

             // Set local interface for outbound multicast datagrams 
             localInterface.s_addr = inet_addr(multicast_address.c_str());
             if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) < 0)
             {
                 cerr << "Error setsockop Setting local interface";
                 exit(EXIT_FAILURE);
             }

            // Set option to receive replies from clients on the same host
            unsigned char loop;
            loop = 1;
            if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0)
            {
                 cerr << "Error setsockopt loop";
                 exit(EXIT_FAILURE);
            }

            // Set the broadcast option
            int so_broadcast = true;
            if (setsockopt(sd, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)) != 0) 
            {
                cerr << "Error setsockopt Failed to set broadcast option";
                exit(EXIT_FAILURE);
            }

            // Set option for ttl to expand current subnet
            unsigned char ttl;
            ttl = 4;
            if (setsockopt(sd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
            {
                cerr << "Error setsockopt ttl";
                exit (EXIT_FAILURE);
            }

            // Send the sync message to the multicast group 
            size_t datalen = sizeof(ClockSyncMessage);
            char* data_= reinterpret_cast<char*>(&oBroadcastMessage);
            if (sendto(sd, data_, datalen, 0, (struct sockaddr*)&groupSock, sizeof(groupSock)) < 0)
            {
                cerr << "Error Sending datagram message in multicast";
            }

            // Set a non-blocking recv time out if there is no data incoming
            struct timeval read_timeout;
            read_timeout.tv_sec = 0;
            read_timeout.tv_usec = 50000;
            if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout))
            {
                cerr << "Error setsockopt recv timeout";
                exit(EXIT_FAILURE);
            }

            // Receiving messages back from multiple clients
            size_t bytes_recv {0};
            while ((bytes_recv = recvfrom(sd, &recv_buffer_, datalen, 0, NULL, NULL)) > 0 && bytes_recv == datalen) 
            {
                // Process Received unicast message from client
                ReceiveHandler (bytes_recv);
            }

            // Release the descriptor back to the OS
            close(sd);
       }

       // Receive Handler of incoming reply messages
       void ReceiveHandler (size_t bytes_recvd)
       {
            // Lock while processing sync message 
            mutex omutex;
            lock_guard<mutex> lock(omutex);

            // Get the immediate (client) time stamp when server message is received
            uint64_t FinalTimeStamp = GetCurrentTimeSinceEpoch();

            // Get a pointer to the received sync message
            ClockSyncMessage* oReceivedMessage = reinterpret_cast<ClockSyncMessage*>(recv_buffer_);
                  
            // Validate the message before processing (in case of mangling per packet drops)
            if (ValidateCheckSum (*oReceivedMessage))
            {
                // Process the (client-time-stamped) received message
                ProcessReceivedMessage (*oReceivedMessage, FinalTimeStamp);
            }

            // Clear the reception buffer
            memset (recv_buffer_,0,max_length_recv);
       }

       // Process received Sync message from clients
       void ProcessReceivedMessage (ClockSyncMessage const & poReceivedMsg, uint64_t pFinalTimeStamp)
       {
            // Print message received from client
            PrintSyncMessage("PROCD("+to_string(++message_count)+")",poReceivedMsg);

            // Compute the offset
            int64_t offset_us = (pFinalTimeStamp + poReceivedMsg.server_ts)/2 - poReceivedMsg.client_ts;

            // Add offset for this client to the stats processor
            stats.AddPoint (poReceivedMsg.clock_id, offset_us);
       }
 
       // Build Sync Message to be broadcast
       ClockSyncMessage BuildBroadcastMessage (void) 
       {
           // Declare response message
           ClockSyncMessage oBroadcastMsg;

           // Set up the broadcast message
           oBroadcastMsg.clock_id  = clock_id;
           oBroadcastMsg.server_ts = GetCurrentTimeSinceEpoch();
           oBroadcastMsg.client_ts = 0;
           oBroadcastMsg.checksum  = ComputeCheckSum(oBroadcastMsg);

           // return built message 
           return oBroadcastMsg;
       }

       // Event handler for printing statistics periodically
       void ProcessStatistics() 
       {
            // Indicate a new statistics period
            cerr << "\nSTAT: Persisting Statistcs for this last minute ... \n";

            // Persist statistics to file
            stats.RecordStatistics();
       }
};

// ****************************
// Main entry point
// ****************************
int main(int argc, char* argv[])
{
  try
  {
      // Check for the required input parameters
      if (argc < 2)
      {
          cerr << "\nUsage: clock_server <clock_id> [interval]\n";
          return -1;
      }

      // Declare the clock id and optional interval (default 10 seconds)
      uint32_t interval {10};
      uint32_t clock_id = atoi(argv[1]);

      // Check if the optional interval has been specified
      if (argc > 2)
      {
          interval = atoi(argv[2]);
      }

      // Declare the multicast clock server object
      clock_server clock (clock_id, interval);

      // Start multicasting from the clock server
      clock.StartBroadcasting ();
  }
  catch (exception& e)
  {
      cerr << e.what() << endl;
  }

  return 0;
}
