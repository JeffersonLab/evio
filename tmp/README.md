
Here is how to convert the CLAS12 pcap files into evio and then into ROOT trees.

Note that a single pcap file has packets destined for 24 different ports. Each stream maintains its own EVIO format so the streams must be split and then the payloads extracted in order to get valid EVIO.

## Get the pcap headers and libraries

This is a round-about way of getting these on the ifarm. If the libpcap-devel package were installed there, then this would not be needed. It is a quick and dirty recipe though for getting these by installing the package in a docker container that is using the same almalinux 9.5 OS that the ifarm is currently using. The files are just copied from there. Skip this if you already have them through other means.
~~~bash
mkdir -p opt/include opt/lib64
podman run -it --rm -v $PWD/opt:/work docker://almalinux:9.5 bash

dnf install -y 'dnf-command(config-manager)'
dnf config-manager --set-enabled crb
dnf install -y libpcap-devel

cp -rp /usr/include/*pcap* /work/include
cp -rp /usr/lib64/*pcap* /work/lib64
exit
~~~

## Build the first two tools.

Clone the existing SRO-RTDP repository and build the pcapSplit and pcpa2evio tools.

~~~bash
git clone git@github.com:JeffersonLab/SRO-RTDP

g++ -g -std=c++2a -o pcapSplit -Iopt/include -Lopt/lib64 SRO-RTDP/src/utilities/cpp/pcapSplit/pcapSplit.cc -lpcap
g++ -g -std=c++2a -o pcap2evio -Iopt/include -Lopt/lib64 SRO-RTDP/src/utilities/cpp/pcap2evio/pcap2evio.cc -lpcap
~~~

## Convert into EVIO
Here are commands to split the captured *.pcap* file and then convert one of them into *evio*. You will only need to run the split command once as that should create 24 *pcap* files. One for each stream. The *pcap2evio* command will need to be run on each single split file you want to process. Thus, if you process all of them, you will end up with 24 *evio* files.
~~~bash
jcache get /mss/epsci/RTDP/2024.05.16.RTDP_CLAS12/CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap
ln -s /cache/epsci/RTDP/2024.05.16.RTDP_CLAS12/CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap
./pcapSplit CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap

# Repeat this for each port
./pcap2evio CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap_split/port7001.pcap 
~~~

## Convert the EVIO file(s) into *ROOT*
This is just an example of reading an *evio* created in the previous step and plotting one of the braches.

n.b. This does minimal EVIO parsing and does *not* use the EVIO library. It should be easy to though, but it will need to use the EVIO-4 library routines. This is because the VTP puts out EVIO-4 so that is what was captured.
~~~bash
module use /cvmfs/oasis.opensciencegrid.org/jlab/scicomp/sw/el9/modulefiles
module load root/6.30.06-gcc11.4.0
g++ -g -std=c++17 -o evio2root SRO-RTDP/src/utilities/cpp/evio2root/evio2root.cc  `root-config --cflags --libs`

# Repeat this for each port
./evio2root CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap_split/port7001.evio

root -l CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap_split/port7001.evio.root
root [1] dcrb->Draw("t")
~~~

<hr>

## Build EVIO

This is an attempt to use the official EVIO library v6.0.1 to read the *evio* file(s) produced by the above.

First, checkout and build the EVIO library
~~~bash
git clone git@github.com:JeffersonLab/evio
cd evio
git checkout v6.0.1
cd -

cmake -S evio -B build.evio -DCMAKE_INSTALL_PREFIX=${PWD}/opt -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_STANDARD=20
cmake --build build.evio --target install -j 16
~~~

## Build the evioReadTest program

~~~bash
g++ -g -std=c++2a evioReadTest.cc -o evioReadTest -Iopt/include -Lopt/lib64 -Lopt/lib -levio -leviocc -Wl,-rpath,${PWD}/opt/lib64
~~~


~~~bash
./evioReadTest CLAS12_ECAL_PCAL_DC_2024-05-16_09-07-18.pcap_split/port7001.evio
~~~