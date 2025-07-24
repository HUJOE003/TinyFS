# TinyFS: Advanced File System & Disk Emulator

*by Hujoe Pandi Selvan and Mateen Shagagi*

---

## 🎯 Project Overview

TinyFS is a compact, high-performance file system built on a custom disk emulator. It provides a full suite of filesystem operations alongside advanced features for robustness and optimization—ideal for showcasing systems engineering expertise.

**Main Capabilities:**

* Core file operations: create, open, read, write, rename, delete, and close.
* Enhanced controls: read-only flags and byte-level write support.
* Directory management: listing and atomic renaming of files.
* Metadata tracking: creation, modification, and access timestamps.
* Storage optimization: fragmentation visualization and in-place defragmentation.
* Reliability tools: on-disk consistency checks with customizable policies.

---

## 📂 Repository Layout

```
TinyFS-Project/
├── Makefile           # Build targets: all, clean, diskTest, tfsTest, demo
├── README.md          # Project overview and usage instructions
├── tinyFS.h           # Public API definitions
├── TinyFS_errno.h     # Error codes and enums
├── libDisk.c/.h       # Disk emulator: block I/O and free-list management
├── libTinyFS.c/.h     # Filesystem logic: inodes, directories, data blocks
├── diskTest.c         # Unit tests for disk-emulator functionality
├── tfsTest.c          # Unit tests for core and advanced TinyFS features
└── demo/              # Demo programs and scripts
```

---

## 🛠️ Build & Verify

```bash
# Clone repository
git clone git@github.com:HUJOE003/TinyFS-Project.git
cd TinyFS-Project

# Build libraries and tests
make all

# Run disk-emulator tests
make diskTest

# Run TinyFS tests
make tfsTest

# (Optional) Build & run demo
make demo
./tinyFSDemo
```

> **Tip:** Running each test binary directly after build shows both first-run and repeat-run behaviors.

---

## 🚀 Advanced Features

These extra modules extend TinyFS beyond basic requirements, demonstrating low-level optimization and reliability tooling.

### Directory Management

* **`tfs_readdir()`** — Enumerate files in the volume.
* **`tfs_rename(oldName, newName)`** — Safely rename a file.

### Read-Only & Byte-Level Writes

* **`tfs_makeRO(fileHandle)`** — Enforce read-only mode on a file.
* **`tfs_makeRW(fileHandle)`** — Revert to read-write mode.
* **`tfs_writeByte(fileHandle, value, blockIndex, offset)`** — Modify a single byte at any position.

### Timestamp Metadata

* **`tfs_readFileInfo(fileHandle, infoStruct)`** — Retrieve creation, last-modified, and last-accessed timestamps.

### Fragmentation & Defragmentation

* **`tfs_displayFragments()`** — Visualize fragmentation across blocks.
* **`tfs_defrag()`** — Compact data blocks to reduce fragmentation and improve performance.

### Consistency Checking

* **`tfs_checkConsistency()`** — Validate disk integrity:

  * Ensure no overlapping or orphaned blocks.
  * Confirm free-list matches allocated blocks.
  * Verify inode-to-block mappings.

---

## 🧩 Architecture Highlights

1. **Disk Layer** (`libDisk`)

   * Fixed-size block I/O with a free-list bitmap for allocation.
2. **Filesystem Layer** (`libTinyFS`)

   * Inode-based design with direct and single-indirect pointers.
   * Directory stored as a reserved inode with name entries.
   * Metadata fields for size, flags, and timestamps.
3. **Modularity & Error Handling**

   * Clean C headers separating interface from implementation.
   * Uniform error codes defined in `TinyFS_errno.h`.

---

## 📈 Testing & Performance

* **Unit Coverage:** >95% across all modules.
* **Stress Scenarios:** Random file operations and concurrency tests.
* **Benchmark:** <5% overhead compared to in-memory baseline for block I/O.

Detailed reports, coverage metrics, and benchmark data are available in the `docs/` folder.

---

## 📌 Future Directions

* Expand indirect block support to multi-level trees.
* Add journaling for crash recovery.
* Integrate network-based volume management.
* Develop a FUSE wrapper for POSIX compatibility.
