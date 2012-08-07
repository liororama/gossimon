This is gossimon, the distributed gossip based, open-source information system
Gossimon is distributed under the BSD License, see Copyright.txt.


Readme Index:
* Description
* History
* Gossimon Programs
* Supported Linux Distributions
* Compilation
* Installation



Description: 
------------
Gossimon is a gossip based distributed monitoring system for a cluster of 
Linux nodes. Each node in the cluster periodically send information about 
itself and others to a randomly selected node. This way each node constantly 
receive information about cluster nodes. This information is locally 
maintained (constantly updated) by each node and can be used by various 
clients. 


History:
---------
Gossimon (previously known as Infod) was initially developed by Lior Amar 
and Ilan Peer at the Distributed computing lab managed by Prof Amnon Barak.
Infod was used as the information layer of the MOSIX cluster operating system.
During time Infod proved to be a valuable tool for any type of cluster and 
support was added for general Linux based clusters.

Gossimon was developed and maintained by ClusterLogic Ltd between the years 
2009-2011 (www.clusterlogic.net). During those years gossimon was release as
an open source project under the BSD license (after getting the permission to
do so from Prof Amnon Barak).
Once ClusterLogic Ltd cease to exist the development was continued by 
Lior Amar (liororama@gmail.com)

Currently gossimon is developed and maintained by Lior Amar as an open source 
project under the BSD license.


Gossimon programs:
------------------

The following programs are part of the gossimon package:

1. /usr/sbin/infod:
   This program should run in the background in each node: it collects
   and propagates information such as load, memory, speed, user usage,
   machine status, etc.  The collected information can then be used
   by the client application below.
   The sys-admin can also add externally collected information.
   This program must run with root privileges. 

2. /usr/bin/mmon:
   A curses based graphical tool that presents information about
   the nodes in the cluster. The file 
   /usr/share/doc/gossimon/mmon.conf.example is a sample 
   color scheme configuration file. This file select the colors used
   by mmon for different displays.

3. /usr/bin/infod-client:
   A utility to query a node and output in XML format what it knows
   about other nodes.

4. /usr/sbin/infod-ctl:
   An administator's tool to control/query infod - modify its algorithm,
   query its status, etc.
   This utility requires root privileges. 

5. /usr/bin/infod-best:
   Find the least loaded node(s) in the cluster (it can be used for
   assigning jobs in the cluster).

6. /usr/bin/testload:
   A program that generates load and consumes memory in a controlled way.
   This program is intended for generating artificial load (cpu/mem/disk) 
   so the gossimon system can be tested

7. /usr/bin/repeat-bg:
   A script for running in the background multiple copies of
   the same program.



Supported Linux distributions:
------------------------------
Gossimon was successfuly compiled and installed on the following:

* Ubuntu 10.04, 12.04 (64bit)
* Centos 5.5, 5.6, 5.7  (64bit)

Feel free to send me a short email if you manage to install it on other 
systems (liororama@gmail.com).




Compilation:
------------
This part is relevant only in case you are trying to install the source 
package.



1) Install required software:

  1.1. Install CMake (http://www.cmake.org) on your node. 
      o On Debain/Ubuntu systems, "apt-get install cmake" would do the trick
      o On other systems: look for a package, download a precompiled tar ball 
        or be bold and compile cmake yourself (it is a very easy task.. :-))

  1.2. Install libxml2
       o Debian/Ubuntu: apt-get install libxml2 libxml2-dev
       o RH based systems: yum install libxml2 libxml2-devel 

  1.3. Install libglib-2.0 
       o Debian/Ubuntu: apt-get install  libglib2.0-0 libglib2.0-dev
       o RH based systems: yum install glib2 glib2-devel
 
  1.4. Install libxml++
       o Debian/Ubuntu: apt-get install libxml++2.6-dev
	
       o RH based: yum install libxml++-devel.x86_64
		   yum install glibmm24-deve

  1.5 Install libxml++
       o Debian/Ubuntu apt-get install libxml++2.6-dev
       o RH based: You might need to install a repository like rpmforge that
         contains the packages (does not appear in the regular repo for Centos5
                   yum install glibmm24-devel
       	    	   yum install libxml++-devel

  1.6 Install cppunit
       o Debian/Ubuntu apt-get install libcppunit-dev
       o RH based: yum install cppunit-devel
  
2. Open the gossimon tarball (once you download it...)
   tar xvf gossimon-1.8.3-Source.tar.gz

3. Configure via cmake:
   cd gossimon-1.8.3-Source.tar.gz
   > cmake .

   In case you see errors regarding missing required libraries, go an install
   the missing libraries...
   
   Note: on Centos 5 you shoud install gcc44 (including g++44). You will need
   to tell cmake to use these compilers instead of the normal gcc

   COMPILER_CMAKE_FLAGS="-D CMAKE_C_COMPILER=gcc44 -D CMAKE_CXX_COMPILER=g++44"
   cmake $COMPILER_CMAKE_FLAGS -DCMAKE_INSTALL_PREFIX=/usr \
      -DGOSSIMON_INSTALL_ETC=/etc .


4. Compile:
   make

If all goes O.K. all required binaries are compiled and ready for install




Installation:
-------------
This part is relevant only in case you are trying to install the source package

Note: you need root privilages to install.

1. Run (as root):   
   make install

   The above program will install all binaries and default configuration files
   to proper locations.

2. Edit /etc/gossimon/infod.conf 
   This file contain important definitions the infod daemon will use. 
   In many cases you will need to write down the local ip of the node in the
   myip=IP section.   


3. Make sure the script /etc/init.d/infod is automatically run on boot time.
   o On Debian/Ubuntu systems use:
     >  update-rc.d infod defaults

   o On Redhat bases systems use:
     > /sbin/chkconfig --add infod

   o On othe systems ... either find the apropriate command or generate the 
     links manualy...

   If you need to pass command line argument to infod, you can use the file
   /etc/defaults/infod.


4. Edit the cluster map file: /etc/gossimon/infod.map
   Make sure the file contains the list of nodes you wish infod the monitor. 

   Note: This file should be the same on all cluster node.

   The file format is simple and is covered in the infod man page.

   Lets look on a simple example. Assume you have a 25 nodes in your cluster.
   The nodes are named n1,n2,....n25. Also assume the IP address of the nodes
   are consecutive. Meaning for example
   n1  192.168.1.101
   n2  192.168.1.102
   ..
   n25 192.168.1.125

   An infod.map file for this cluster will look like the example below

   infod.map file example:
   ----------------- Example start --------------------------------
   # A comment
   # The following line defines a cluster with 25 nodes with consecutive
   # IP addresses. The first node is # n1 which gets id 1; the second
   # is mos2 with id 2, etc.
   
   1  n1 25
   
   ------------------ Example End ----------------------------------

5. Start infod in all the nodes in your cluster.
   Just run the command /etc/init.d/infod start

   Make sure the infod daemon actually starts.
   ps auxww | grep infod

   You are supposed to see 2 processes.
   For example:
   >ps auxww |grep infod
   root     32534  0.0  0.6  17768  3452 ?        SLs  11:26   0:00 infod
   root     32535  0.0  0.3  18500  1664 ?        S    11:26   0:00 infod

   If there are no infod processes in the output of ps, try to look in the log
   files (/var/log/daemon on Debian/Ubuntu or /var/log/messages on RH based)



6. To view the status of your cluster, either run "mmon" in one of the
   cluster nodes, or "mmon -h <node-name>" from outside the cluster.






