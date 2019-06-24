#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <chrono>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "clock_utils.hpp"

using boost::asio::ip::udp;
using boost::asio::ip::address;

using namespace std;
//
//***********************************************************************************************
//
// Class clock_client: Multicast listening client for clock synchronization
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

        // Declare error code
        boost::system::error_code err;

        // Declare receiver socket and service 
        boost::asio::io_service io_service1;
        boost::asio::ip::udp::socket socket_ {io_service1};

        // Declare sender endpoint 
        boost::asio::ip::udp::endpoint sender_endpoint_;

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
             // Start receiving
             StartReceiving_impl (boost::asio::ip::address::from_string("0.0.0.0"), boost::asio::ip::address::from_string(cstrMulticastAddress));

             // Listen to service
             io_service1.run();
        }

private:

        // Start receiving implementation
        void StartReceiving_impl (const boost::asio::ip::address& listen_address, const boost::asio::ip::address& multicast_address)
        {
             // Open and bind the socket for the listening endpoint
             boost::asio::ip::udp::endpoint listen_endpoint( listen_address, multicast_port);
             socket_.open(listen_endpoint.protocol());
             socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
             socket_.bind(listen_endpoint);

             // Join the multicast group
             socket_.set_option(boost::asio::ip::multicast::join_group(multicast_address));

             // Set up the receive handler
             socket_.async_receive_from (
                                          boost::asio::buffer(data_, max_length), 
                                          sender_endpoint_, 
                                          boost::bind(&clock_client::ReceiveHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
                                        );
       }

       // Event Handler for receicing multicasted messages
       void ReceiveHandler (const boost::system::error_code& error, size_t bytes_recvd)
       {
         try {

            // Check for errors
            if (!error)
            {
                // Get the immediate (client) time stamp when server message is received 
                uint64_t TimeStamp = GetCurrentTimeSinceEpoch();
                  
                // Check if message is fully received
                if (bytes_recvd == sizeof(ClockSyncMessage)) 
                {
                    // Get a pointer to the received sync message
                    ClockSyncMessage* oReceivedMessage = reinterpret_cast<ClockSyncMessage*>(data_);
#if !defined NO_PRINT
		    // Indicate Message
		    PrintSyncMessage("recvd", *oReceivedMessage);
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

                // Continue async read
                socket_.async_receive_from(
                                            boost::asio::buffer(data_, max_length), 
                                            sender_endpoint_,
                                            boost::bind(&clock_client::ReceiveHandler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
                                          );
              }
              else 
              {
                   cerr << "Error Receive Handler: Client [" << client_id << "] Error [" << error << "]" << endl;
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
      bool SendMessage(ClockSyncMessage const &poMsg)
      {
           // Check if socket is open
           if (socket_.is_open())
           {
               // Send sync message reply on the opened socket
               socket_.send_to(boost::asio::buffer(&poMsg,sizeof(poMsg)), sender_endpoint_, 0, err);

#if !defined NO_PRINT
	       // Indicate Message
      	       PrintSyncMessage("sentm", poMsg);
#endif

               // Check error code
               if (!err)
               {
                   // Successful send
                   return true;
               }
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

