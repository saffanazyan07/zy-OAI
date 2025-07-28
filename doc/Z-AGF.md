# 🚀 z-agf + z-cu Integration Guide

This guide provides step-by-step instructions on setting up, building, and running the `z-cu` and `z-agf` components of the zy-OAI project.

---

## 📁 Project Structure

Your folder should be structured as follows:

zy-cu/
├── CMakeLists.txt
├── executables/
├── openair3/
│ └── ocp-gtpu/
│ └── zy-agf/ ← Contains z-agf source code or submodule
├── build/
└── ...

yaml
Copy
Edit

---

## 🔧 Step 1: Clone the Repositories

### Clone the main project (`z-cu`)

```bash
git clone https://github.com/saffanazyan07/zy-OAI.git zy-cu
cd zy-cu
```
Option A: If using submodules (recommended)
```bash
git submodule update --init --recursive
```
Option B: If copying manually
```
cd openair3/ocp-gtpu
git clone -b master https://github.com/saffanazyan07/zy-agf.git
```
⚙️ Step 2: Build the Components
🔨 Build z-cu
```
cd zy-cu
# Run your CU build process here (replace with actual build command)
make cu
```
🔨 Build z-agf
```
cd openair3/ocp-gtpu/zy-agf
chmod +x run_z-agf.sh
./run_z-agf.sh
```
If needed, adjust environment variables and install dependencies before running the build.

🚀 Step 3: Run the Components
▶️ Run z-cu
```
cd zy-cu
./executables/z-cu-binary  # Replace with your actual binary name
```
▶️ Run z-agf
```
cd openair3/ocp-gtpu/zy-agf
./run_z-agf.sh
```
You may need to run chmod +x on any other scripts or binaries if they’re not executable.

🧪 Step 4: Test Connectivity
Make sure both components are communicating correctly. Example:

```
ping -I zcu-interface 10.0.0.2
```
Replace interface and IP address with actual configuration used in your setup.

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
