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


---

## 🔧 Step 1: Clone the Repositories

### Clone the main project (`z-cu`)

```bash
git clone https://github.com/saffanazyan07/zy-OAI.git zy-cu
cd zy-cu
```
