#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>

using namespace std;
//
//***********************************************************************************************
//
// Utility functions and structures for clock client/server time skews
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
// Synchronization message
struct ClockSyncMessage 
{
      uint32_t clock_id; // or client_id
      uint64_t server_ts;
      uint64_t client_ts;
      uint16_t checksum;
};

// Get the current time (in microseconds) since epoch
uint64_t GetCurrentTimeSinceEpoch (void) { return chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count(); }

// Computation of checksum for synchronization message
int16_t ComputeCheckSum (ClockSyncMessage const &poMsg)
{
       // Declare checksum to be computed
       uint16_t checksum {0};

       // If non zero, cummulative byte add (clockid) 
       if (poMsg.clock_id != 0) 
       {
           for (unsigned i=0; i<sizeof(poMsg.clock_id); i++) 
           {
                checksum += ((poMsg.clock_id >> i * 8) & 0x000000ff);
           }
       }

       // If non zero, cummulative byte add (server_ts) 
       if (poMsg.server_ts != 0)
       {
           for (unsigned i=0; i<sizeof(poMsg.server_ts); i++) 
           {
                checksum += ((poMsg.server_ts >> i * 8) & 0x00000000000000ff);
           }
       }

       // If non zero, cummulative byte add (client_ts) 
       if (poMsg.client_ts != 0) 
       {
           for (unsigned i=0; i<sizeof(poMsg.client_ts); i++) 
           {
                checksum += ((poMsg.client_ts >> i * 8) & 0x00000000000000ff);
           }
       }

       return checksum;
}

// Validate message checksum (to check for packets integrity under udp, etc.)
bool ValidateCheckSum(ClockSyncMessage const &poMsg) 
{
     return poMsg.checksum == ComputeCheckSum(poMsg);
}

// Print Sync Message for tracking content
void PrintSyncMessage(string psLegend, ClockSyncMessage const & poMsg) 
{
     mutex mx;
     lock_guard<mutex> lck(mx);

     cerr << hex << setfill('0') << "\n" << psLegend << ": clock_id [0x"
		 << setw(8) << poMsg.clock_id <<  "] client_ts [0x"
		 << setw(16) << poMsg.client_ts << "] server_ts [0x"
		 << setw(16) << poMsg.server_ts << "] checksum [0x"
		 << setw(4) << poMsg.checksum <<  "]" << dec << endl;
}
