#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std;
//
//***********************************************************************************************
//
// Class clock_timer: Timer class to process periodic sync message broadcast and statistics
//
// Ernesto L Aparcedo, Ph.D. (c) 2019 - All Rights Reserved.
//
//***********************************************************************************************
//
class clock_timer
{
        // Declare atomic for state
	atomic<bool> exec;

        // Declare timer thread
	thread oTimerThread;

        // Declare mutex for timer firing
	mutex timer_interval_mutex;

        // Declare timer interval
	int timer_interval;

public:
	// Constructor
	clock_timer () : exec(false) {}

	// Destructor
	~clock_timer()
	{
            // If memory acquired then stop
	    if (exec.load(memory_order_acquire))
	    {
		stop();
	    }
	}

	// Get the timer interval
	int GetInterval()
	{
	    lock_guard<mutex> lock(timer_interval_mutex);
	    return timer_interval;
	}

	// Set the timer interval
	void SetInterval(int piInterval)
	{
	     lock_guard<mutex> lock(timer_interval_mutex);
	     timer_interval = piInterval;
	}

	// Increment the timer interval
	void IncrementInterval(int piIntervalIncrement = 1)
	{
	     lock_guard<mutex> lock(timer_interval_mutex);
	     timer_interval = (timer_interval + piIntervalIncrement);
	}

	// Stop the timer 
	void stop()
	{
             // Release memory of atomic
	     exec.store(false, memory_order_release);

             // Join timer thread
	     if (oTimerThread.joinable())
	     {
	         oTimerThread.join();
	     }
	}

	// Kill the timer
	void kill()
	{
             // If memory acquired then stop
	     if (exec.load(memory_order_acquire))
	     {
                 // Release memory of atomic
	         exec.store(false, memory_order_release);
             }		
	}

	// Start the thread and callback for the specified milliseconds
	void start(int pinterval, function<void(void)> func)
	{
	      // Set the interval as input
	      SetInterval(pinterval);

              // Stop timer if already acquired
	      if (exec.load(memory_order_acquire))
	      {
		  stop();
	      }

              // Set the atomic to proceed to lauch timer thread
	      exec.store(true, memory_order_release);

              // Launch timer thread
	      oTimerThread = thread
	      (
		   [this, func]()
	           {
			while (true)
			{
                                // Sleep for interval then invoke callback
				this_thread::sleep_for(chrono::milliseconds(GetInterval()));

				func();
			}
		   }
	      );
	}

	// Check if timer thread is running
	bool is_running() const noexcept
	{
	     return (exec.load(memory_order_acquire) && oTimerThread.joinable());
	}
};
