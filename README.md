# clockserver
A multicast monitoring system where a central server tracks the clock skew of several clients

Requirements:
-------------

- Compiled and tested under Ubuntu v18.04 and g++ v7.3 and Windows 10 Professional and Visual Studio 2017 v15.9.4
- Uses GLIBC v2.27 for multicasting. An optional version using Boost/asio is also provided.
- Written in standard C++11. Enforced via compilation flag
- Binaries for Ubuntu 18.4 are included
- Multicast is provided via multicast address 238.10.50.50 port 5000

Files Attached:
--------------

- clock_client_glibc.cpp : Receiving clock client (multicast message receiver). Uses GLIBC.
- clock_server_glibc.cpp : Multicast clock Server (multicast message sender). Uses GLIBC.
- clock_client_boost.cpp : Receiving clock client (multicast message receiver). Uses Boost/asio. Optional Alternative.
- clock_server_boost.cpp : Multicast clock Server (multicast message sender). Uses Boost/asio. Optional Alternative.
- clock_timer.hpp        : Periodic custom timer to perform statistics and message broadcasting (by clock_server)
- clock_stats.hpp        : Statistics processor of time skews (offsets)
- clock_utils.hpp        : Generals functions and structures
- Makefile               : Make file for constructing binaries (for use with linux make utility). GCLIB version.
- clock_server.out       : Sample output for 5 mins under 2 receiving clients. Executed with GLIBC version.
- clock_server.500clients.out : Sample output for 5 mins for various client populations up to 500 receiving clients. Executed with GLIBC version.
- clock_client_glibc     : Clock client Ubuntu executable. Uses GLIBC.
- clock_server_glibc     : Clock server Ubuntu executable. Uses GLIBC.
- README                 : This file.

Testing:
-------

- See clock_server.out for requested 5 mins testing for 2 clients
- Tested 100 and 500 clients in an Intel i3-7100 at 3.9Ghz box with 4 gb ram. All processes running in Ubuntu.
- Tested clock_server_glibc under Windows 10 with 100 clients running in Ubuntu

Ernesto L. Aparcedo, Ph.D. - (c) 2019 - All Rights Reserved.
