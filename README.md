# 🚀 z-agf + z-cu Integration Guide

This guide provides step-by-step instructions on setting up, building, and running the `z-cu` and `z-agf` components of the zy-OAI project.

---

## 📁 Project Structure

Your folder should be structured as follows:
```
zy-cu/
├── CMakeLists.txt
├── executables/
├── openair3/
│   └── ocp-gtpu/
│       └── zy-agf/         ← ✅ z-agf
├── build/
└── ...

```
---

## 🖥️ Deployment Note: Use 2 Separate VMs

> ❗ **Important:** `z-cu` and `z-agf` must not run on the same virtual machine (VM).

To ensure correct network behavior and avoid interface conflicts, you must deploy:

- `z-cu` on **VM #2**
- `z-agf` on **VM #3**

Both VMs should be on the same network bridge or have a reachable route to each other.

### 🔌 Network Configuration Example

| Component | VM |Purpose | Interface | Example IP        |
|-----------|----|--------|-----------|-------------------|
| 5gc       | VM1| N2 / N3     | `enp0s8`    | `192.168.60.2`   |
| z-cu      | VM2| N2 / N3     | `enp0s8`    | `192.168.60.8`   |
|           |    | F1          | `enp0s9`    | `192.168.60.88`   |
| z-agf     | VM3| F1          | `enp0s9`    | `192.168.60.77`   |

Make sure each component is configured to connect to the peer’s IP and port accordingly.

## 🔧 Step 1: Clone the Repositories

### Clone the main project (`z-cu`)

```bash
git clone https://github.com/saffanazyan07/zy-OAI.git zy-cu
cd zy-cu
```
> You can skip this process
Option A: If using submodules (recommended)
```bash
git submodule update --init --recursive
```
Option B: If copying manually
```
cd openair3/ocp-gtpu
git clone -b master https://github.com/saffanazyan07/zy-agf.git
```
## ⚙️ Step 2: Configuration Setup
### 🔨 config z-cu 
```
cd zy-cu
sudo nano targets/PROJECTS/GENERIC-NR-5GC/CONF/cu_gnb.conf
```
### set mcc/mnc N2/N3 F1 interface IP
```
gNBs =
(
 {
    ////////// Identification parameters:
    gNB_ID = 0xe00;

#     cell_type =  "CELL_MACRO_GNB";

    gNB_name  =  "gNB-Eurecom-CU";

    // Tracking area code, 0x0000 and 0xfffe are reserved values
    tracking_area_code  =  1;
    plmn_list = ({ mcc = 208; mnc = 99; mnc_length = 2; snssaiList = ({ sst = 1 }) });

    nr_cellid = 12345678L;

    tr_s_preference = "f1";

    local_s_address = "192.168.60.88"; //IP F1 interface CU
    remote_s_address = "0.0.0.0"; // IP F1 interface DU
    local_s_portc   = 501;
    local_s_portd   = 2152;
    remote_s_portc  = 500;
    remote_s_portd  = 2152;

    # ------- SCTP definitions
    SCTP :
    {
        # Number of streams to use in input/output
        SCTP_INSTREAMS  = 2;
        SCTP_OUTSTREAMS = 2;
    };


    ////////// AMF parameters:
    amf_ip_address = ({ ipv4 = "192.168.60.2"; }); // IP N2 interface 5GC 

    NETWORK_INTERFACES :
    {

        GNB_IPV4_ADDRESS_FOR_NG_AMF              = "192.168.60.8"; //IP N2 interface CU, it must different with F1 interface of CU
        GNB_IPV4_ADDRESS_FOR_NGU                 = "192.168.60.8"; //IP N3 interface CU
        GNB_PORT_FOR_S1U                         = 2152; # Spec 2152
    };
  }
);
```
### set tunnel for local connection
open directory
```
cd zy-cu
sudo nano openair3/ocp-gtpu/gtp_itf.cpp
```
change to your F1 IP config (support for 2 z-agf(77,99))
```
gtpu_tunnel_t gtpu_tunnels[MAX_TUNNELS] = {
  {"192.168.60.88", "192.168.60.77", 2154, -1, -1, 0x00000001, PTHREAD_MUTEX_INITIALIZER}, // TEID = 1 
  {"192.168.60.88", "192.168.60.99", 2153, -1, -1, 0x00000002, PTHREAD_MUTEX_INITIALIZER}  // TEID = 2
};
```
> noted: z-agf same as cu, left side for local IP  
## ⚙️ Step 3: Build the Components
🔨 Build z-cu 
```
cd zy-cu/cmake_targets
# Run your CU build process here
./build_oai --z-cu -I
```
🔨 Build z-agf
```
cd zy-cu/cmake_targets
# Run your CU build process here
./build_oai --z-agf -I
cd openair3/ocp-gtpu/zy-agf
chmod +x run_z-agf.sh
./run_z-agf.sh
```
If needed, adjust environment variables and install dependencies before running the build.

## 🚀 Step 4: Run the Components
▶️ Run z-cu
```
cd zy-cu
chmod +x run_cu.sh
./run_cu.sh
```
▶️ Run z-agf
```
cd openair3/ocp-gtpu/zy-agf
./run_z-agf.sh
```
You may need to run chmod +x on any other scripts or binaries if they’re not executable.

📝 Notes
Make sure all required system packages and libraries are installed.
Interface naming and IP addressing must match between z-cu and z-agf.
Log files (if any) will typically appear in log/ or output/ directories.

📬 Contact
Author: saffanazyan07
Email: saffanazyan07@gmail.com

Feel free to raise issues or pull requests in the repository for enhancements or bug fixes.

✅ Future Enhancements
 Dockerize the z-agf component
 Auto-start scripts using systemd
 Add integration testing script
