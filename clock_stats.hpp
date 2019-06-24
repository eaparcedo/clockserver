#include <iostream>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <algorithm>
#include <utility>
#include <thread>
#include <mutex>
#include <numeric>

using namespace std;
//
//***********************************************************************************************
//
// Class clock_stats: Calculating statistics in clock client/server time skew
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
class clock_stats {

    // Declare file to which stats are persisted
    const string cstrFileName {"./clock_server.out"};

    // Declare mutex to arbitrate taking stats and adding to sample population
    mutex mx;

    // Declare the output file 
    ofstream out_file;

    // Declare the statistics cumulative collection
    map<uint32_t, vector <int64_t>> stats;

public:

    // Constructor
    clock_stats() {}

    // Clear stats collection
    void Clear ()
    {
         stats.clear();
    }

    // Add point to statistics collection
    void AddPoint(uint32_t pClockID, int64_t offset) 
    {
        // Lock while processing this point 
        lock_guard<mutex> lock(mx);

         // Check if this client is already available
         if (stats.count(pClockID) > 0) 
         {
             // Add a new point to this clock client
             stats[pClockID].push_back(offset);
         }
         else 
         {
             // Add a new collection for this new client and add the current point
             vector<int64_t> vec;
             vec.push_back(offset);
             stats[pClockID] = vec;
         }
    }

    // Persist statistics to a file 
    void RecordStatistics()
    {
        // Lock while recording stats to avoid contention if adding points
        lock_guard<mutex> lock(mx);

        // Ensure that there is data to report
        if (stats.size() > 0) 
        {
            // Open file
            out_file.open (cstrFileName, ofstream::out | ofstream::app);

            // Declare iterator for clock client collections
            map<uint32_t, vector<int64_t>>::iterator it;

            // Iterate over all collections
            for (it=stats.begin(); it != stats.end(); it++)
            {
                 // Declare the stream for string edit
                 ostringstream stats_stream;

                 // Build the statistics edit line for this specific clock client
                 stats_stream <<  ConvertEpochToTime_us() << "," << to_string(it->first) << "," << ComputeStatistics(it->second);
              
                 // Perform the i/o to the file
                 out_file << stats_stream.str() << endl;
                 out_file.flush();
            }

            // Close the file
            out_file.close();

            // Reset collections for next sample period
            Clear();
        }
    }

    // Compute statistics
    string ComputeStatistics (vector<int64_t> vec)
    {
        // Declare edit for computed stats
        string sEdit;
    
        // Ensure there are elements in the collection
        if (vec.size() > 0) 
        {
            // Compute max and min
            int64_t min_elem = *(min_element(begin(vec), end(vec)));
            int64_t max_elem = *(max_element(begin(vec), end(vec)));

            // Compute average
            double sum  = accumulate(vec.begin(), vec.end(), 0.0);
            double mean = sum / vec.size();
            int64_t avg_elem = static_cast<int64_t>(mean);

            // Compute median
            nth_element(vec.begin(), vec.begin() + vec.size()/2, vec.end());
            int64_t med_elem = vec[vec.size()/2];

            // Compose edit
            ostringstream ostream;
            ostream << setprecision(0) << vec.size() << ","  << min_elem << "," << avg_elem << "," << med_elem << "," << max_elem;
            sEdit = ostream.str();
        }

        return sEdit;
    }

    // Get current microseconds since epoch
    long long get_current_us_epoch() 
    {
	return chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now().time_since_epoch()).count();
    }

    // Convert epoch to datetime edit with microsecond precision
    string ConvertEpochToTime_us()
    {
       // Get Current microseconds since epoch
       long long micSecondsSinceEpoch = get_current_us_epoch();

       // Get time after duration
       const auto durationSinceEpoch = chrono::microseconds(micSecondsSinceEpoch);
       const chrono::time_point<chrono::system_clock> tp_after_duration(durationSinceEpoch);
       time_t time_after_duration = chrono::system_clock::to_time_t(tp_after_duration);
       localtime(&time_after_duration);

       // Calculate remainder in microseconds
       long long int micseconds_remainder = micSecondsSinceEpoch % 1000000;

       // Build date edit
       ostringstream ostring;
       ostring <<put_time(localtime(&time_after_duration), "%Y-%m-%d %H:%M:%S.") << micseconds_remainder;

       // Return edit date with microsecond precision
       return ostring.str();
    }
};
