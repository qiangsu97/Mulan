# **Mulan**
Mulan is a novel system that features SmartNIC as a service to address issues in the current offloading paradigms, including
- Poor cluster-wide resource efficiency
- Inflexible SmartNIC management 
- Inflexible application deployment

by organizing heterogeneous NIC resources as a pool and offering a unified one-NIC abstraction to application developers.
This allows 
- developers to focus solely on the application logic while dynamically specifying their performance targets. 
- operators, now with complete visibility over the SmartNIC cluster, are able to flexibly consolidate applications for overall resource efficiency. 

In particular, Mulan provides a flexible modular programming model, a scalable data plane, and a unified control plane.

## Open-sourcing Schedule
Currently we have released our key components of Mulan, including 
- Programming model with APIs for
  - Packet processing
  - Socket processing 
  - Accelerator function
- Scalable data plane pipeline
- Unified control plane
  - Application orchestration
  
   
More to be released in the next stage.

## Environment
Our cluster comprises:
- Mellanox SN2700 32-Port switch
- NVIDIA BlueField-2 SmartNICs
- NVIDIA BlueField-1 100GbE SmartNICs
- AMD Pensando SmartNICs 
- Client servers 
    - 32 cores AMD EPYC-7542 CPU @ 2.9 GHz 
    - 256 GB of DRAM
    - One NVIDIA ConnectX-6 100GbE NIC (MT28908)

Software configurations:
- DPDK: 20.11.5
- Traffic generator: DPDK-Pktgen 23.03.1


## **Compile & Run**
```bash
make
bash ./run.sh -live 2 10
```
The above step will start Mulan using 2 cores and run the sample program in src/example/ (the program in paper Listing 1) for 10 seconds.

## Repo Structure
* ``rulesets/`` contains rulesets we use for regex accelerator on Bluefield-2 SmartNICs.
* ``src/`` contains source code of Mulan.  
* ``traffic_generator/`` contains sample traffic generation script and pcaps.
