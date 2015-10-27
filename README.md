Builds a Throughput Test Server For Linux
=========================================

The throughput test is a IPv6 based test that streams packets
from a sender node to a connected PC. You will need a server software to
properly use the test.

To use this test as a TCP throughput test server for the default configuration
of the Contiki throughput test run:
./bin/testServer -st --port=3000

For more information on different configuration options use:
./bin/testServer -h

Building
========

Just use 'make' to build on a Linux system. The output binary can be found
in ./bin/
