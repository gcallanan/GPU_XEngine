# MeerKAT GPU X-Engine Prototype

This repository contains the software for a prototype GPU X-Engine designed to operate on the MeerKAT telescope. The main aim of this project is to experiment and determine the best method to build a GPU X-Engine for future MeerKAT correlator upgrades.

This project has a number of different sections:
1. meerkat_gpu_xengine - This folder contains all files required to build the X-Engine prototype software.
2. spead_receiver - This folder contains a striped down version of the meerkat_gpu_xengine. This project only contains the ingest stage and was used for benchmarking the achievable ingest rates on the 40 GbE NIC
3. rx_amp_phase.py, rx_amp_phase_multicast.py, rx_amp_phase_multicast_python2p7.py, rx_real_imag.py - These programs are all designed to display data produced by X-Engines of various implementations
4. f_engine_simulator.cpp - This program will simulate F-Engine data at the required data rate after some tuning.

In order to build the meerkat_gpu_xengine, navigate to the meerkat_gpu_xengine folder and type make. This should be all that is required unless you are missing some dependancies.

The main file for the meerkat_gpu_xengine is meerkat_gpu_xengine/MeerkatGpuXEngine.cpp. This is a good file to start with for trying to trace through the code.
The meerkat_gpu_xengine/global_definitions.h file specifies a few macros, structs and includes that are used throughout the project.
