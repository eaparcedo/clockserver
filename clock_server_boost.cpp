#include <iostream>
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
  
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "clock_utils.hpp"
#include "clock_stats.hpp"
#include "clock_timer.hpp"

using boost::asio::ip::udp;
using boost::asio::ip::address;

using namespace std;
//
//***********************************************************************************************
//
// Class clock_server: Multicast broadcasting server for clock synchronization
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
class clock_server :  public std::enable_shared_from_this<clock_server>{

        // Declare multicast port
        const short multicast_port {5000};

        // Declare multicast address
        const string multicast_address {"238.10.50.50"};

        // Declare error code and service
        boost::system::error_code err;
        boost::asio::io_service io_service;

        // Declare clock id 
        uint32_t clock_id;

        // Declare broadcasting period 
        uint32_t interval;

        // Declare message count
        uint64_t message_count {0};

        // Declare sender endpoint
        shared_ptr<boost::asio::ip::udp::endpoint> sender_endpoint_;

        // Declare receiver socket  
        shared_ptr<boost::asio::ip::udp::socket> socket_;

	// Declare the broadcast timer  
	shared_ptr<clock_timer> oBroadcastTimer;

	// Declare the statistics timer  
	shared_ptr<clock_timer> oStatisticsTimer ;

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
                      sender_endpoint_ (make_shared<boost::asio::ip::udp::endpoint> (boost::asio::ip::address::from_string(multicast_address), multicast_port)),
                      socket_          (make_shared<boost::asio::ip::udp::socket>(io_service, sender_endpoint_->protocol())),
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
             // Start receiving from clients
             StartReceive();

             // Start stats timer (every minute)
             oStatisticsTimer->start(60*1000, bind(&clock_server::ProcessStatistics, this));

             // Start broadcast timer (every interval seconds)
             oBroadcastTimer->start(interval*1000, bind(&clock_server::StartBroadcasting_impl, this));

             // Run the service
             io_service.run();
        }

private:

       // Implementation of Sync Message broadcasting 
       void StartBroadcasting_impl  ()
       {
             // Set up Receive Handler
             SetupReceiveHandler();

             // Build a broadcast message 
             ClockSyncMessage oBroadcastMessage = BuildBroadcastMessage();

             // Indicate message has been built
             PrintSyncMessage("BUILT",oBroadcastMessage);

             // Multicast the message on the sender socket
             socket_->async_send_to( 
                                     boost::asio::buffer(&oBroadcastMessage,sizeof(ClockSyncMessage)),  
                                     *sender_endpoint_,
                                     boost::bind(&clock_server::PostSendHandler, this, boost::asio::placeholders::error)
                                   );

       }

       // Event Handler post sending of multicast message
       void PostSendHandler (const boost::system::error_code& error)
       {
            // Just increment the number of successful messages sent
            ++message_count;


            if (!error)
            {
                cerr << "Message broadcast successfully [" << message_count << "]" << endl;
            }
            else 
            {
                cerr << "Message NOT broadcast successfully [" << message_count << "]" << endl;
            }

       }

       // Start reception of udp content by setting async callback processing
       void StartReceive ()
       {
            // Process the incoming udp payload asynchronously
             socket_->async_receive_from (

                        boost::asio::buffer(recv_buffer_),
                        *sender_endpoint_,
                        boost::bind(&clock_server::ReceiveHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
            );
       }

       // Receive Handler of incoming reply messages
       void ReceiveHandler (const boost::system::error_code& error, size_t bytes_recvd)
       {
            // Lock while processing data 
            mutex omutex;
            lock_guard<mutex> lock(omutex);

            // Check for errors
            if (!error)
            {
                // Get the immediate (client) time stamp when server message is received
                uint64_t FinalTimeStamp = GetCurrentTimeSinceEpoch();

                // Check if message is fully received
                if (bytes_recvd == sizeof(ClockSyncMessage))
                {
                    // Get a pointer to the received sync message
                    ClockSyncMessage* oReceivedMessage = reinterpret_cast<ClockSyncMessage*>(recv_buffer_);
                  
                    // Validate the message before processing (in case of mangling per packet drops)
                    if (ValidateCheckSum (*oReceivedMessage))
                    {
                        // Process the (client-time-stamped) received message
                        ProcessReceivedMessage (*oReceivedMessage, FinalTimeStamp);
                    }

                    // Clear the buffer
                    memset (recv_buffer_,0,max_length_recv);
                }
 
                // Continue async reads
                socket_->async_receive_from ( 
                                              boost::asio::buffer(recv_buffer_),
                                              *sender_endpoint_,
                                              boost::bind(&clock_server::ReceiveHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
                                            );
            }
            else 
            {
                // Write the legend
                this_thread::sleep_for(chrono::milliseconds(10));
                cerr << "Error [" << error << "] Msg [" << error.message() << "]\n";

#if !defined NO_PRINT
                cerr << "\nMulticasting...\n";
#endif

            }
       }

       // Sets up receive handler for clock client responses
       void SetupReceiveHandler () 
       {
             // Reset endpoint              
             socket_->close();
             sender_endpoint_ = make_shared<boost::asio::ip::udp::endpoint> (boost::asio::ip::address::from_string(multicast_address), multicast_port);
             socket_          = make_shared<boost::asio::ip::udp::socket>(io_service, sender_endpoint_->protocol());

             // Set up receive handler for response messages
             StartReceive();
       }

       // Process received Sync message from clients
       void ProcessReceivedMessage (ClockSyncMessage const & poReceivedMsg, uint64_t pFinalTimeStamp)
       {
            // Print message received from client
            PrintSyncMessage("PROCD("+to_string(message_count)+")",poReceivedMsg);

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
