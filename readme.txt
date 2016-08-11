Introduction
------------

Texel is a free software (GPL v3) chess engine written in C++11.

More information about the program is available in the chess programming wiki:

    http://chessprogramming.wikispaces.com/Texel


Pre-compiled binaries
---------------------

The texel distribution contains the following pre-compiled binaries:

texel-arm      : For the armv7-a architecture. Should work on most modern
                 android devices. 
texel32.exe    : For 32-bit windows systems with SSE42 and POPCOUNT. 
texel32old.exe : For 32-bit windows systems without SSE42 and POPCOUNT. 
texel64        : For 64-bit linux intel systems with SSE42 and POPCOUNT. 
texel64.exe    : For 64-bit windows intel systems with SSE42 and POPCOUNT.
texel64amd.exe : For 64-bit windows systems with SSE42 and POPCOUNT. 
texel64old.exe : For 64-bit windows systems without SSE42 and POPCOUNT. 


UCI options
-----------

Hash

  Controls the size of the main (transposition) hash table. Texel supports up to
  512GiB for transposition tables. Other hash tables are also used by the
  program, such as a pawn hash table. These secondary tables are quite small and
  their sizes are not configurable.

MultiPV

  Set to a value larger than 1 to find the N best moves when analyzing a
  position. This setting has no effect when playing games. The GUI normally
  handles this option so the user does not have to set it manually.

OwnBook

  When set to true, Texel uses a small built in opening book. When set to false,
  texel relies on the GUI to handle the opening book.

Ponder

  Texel supports pondering mode, also called permanent brain. In this mode the
  engine calculates also when waiting for the other side to make a move. This
  option changes the time allocation logic to better suit pondering mode. The
  option is normally handled automatically by the GUI.

Strength

  Strength can be smoothly adjusted between playing random legal moves (0) and
  playing at full strength (1000).

Threads

  Texel can use multiple CPUs and cores using threads. Up to 64 threads are
  supported.

UCI_AnalyseMode

  This option is normally set automatically by the GUI when in analysis mode.
  In analysis mode, texel does not use its opening book.


Compiling
---------

The distribution contains a Makefile set up to to compile the program using the
GCC compiler. To build a generic binary that does not require any special CPU
instructions, type "make" in a terminal window.

There are other targets in the Makefile that can be used to build versions
optimized for Intel CPUs, and versions using the POPCNT CPU instruction. See the
Makefile for details.
