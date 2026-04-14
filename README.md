# Multi-Container Runtime with Kernel Memory Monitor

---

##  Overview

This project implements a lightweight container runtime in C along with a Linux kernel module to monitor and control memory usage of containers. The system allows users to run, manage, and terminate multiple container-like processes while enforcing memory limits using kernel-level monitoring.

---

##  Features

* Run multiple containers simultaneously
* List running containers using `ps`
* Stop specific containers
* Kernel-level memory monitoring
* Soft and hard memory limits
* Automatic process termination on exceeding limits
* Persistent container tracking using file storage
* Logging system for container events

---

##  Commands

### ▶ Run Container

```bash
sudo ./engine run <id> <program>
```

###  List Containers

```bash
sudo ./engine ps
```

###  Stop Container

```bash
sudo ./engine stop <id>
```

---

##  Architecture

```
User Space (engine.c)
        ↓
     IOCTL
        ↓
Kernel Space (monitor.c)
```

* `engine.c` → handles user commands (run, ps, stop)
* `monitor.c` → kernel module for memory monitoring
* Communication via IOCTL system calls

---

##  Scheduling Experiment

```bash
nice -n 10 ./cpu_hog
sudo nice -n -5 ./cpu_hog
```

### Observation:

* Lower nice value (-5) → higher priority → faster execution
* Higher nice value (10) → lower priority → slower execution

---

##  Observations

* CPU-bound processes (`cpu_hog`) increase memory slowly
* Memory-bound processes (`memory_hog`) exceed limits quickly
* Soft limit generates warning logs
* Hard limit kills the process
* Kernel monitoring ensures accurate tracking

---

##  Design Decisions

* File-based tracking (`containers.txt`) for persistence
* IOCTL used for kernel communication
* Kernel module ensures precise monitoring
* Simple CLI for usability

---

##  Future Improvements

* Supervisor architecture
* IPC using sockets/FIFO
* Namespace isolation using `clone()`
* Root filesystem isolation
* Bounded buffer logging

---

##  Example Output
## 📸 Example Output

### 🔹 Run and PS
![Run](run.png)

---

### 🔹 Stop Container
![Stop](stop.png)

---

### 🔹 Kernel Logs
![Kernel](dmesg.png)

---

### 🔹 Engine Logs
![Log](log.png)
###  Running Containers & Listing (ps)

![Run and PS](Screenshot 2026-04-14 151026.png)

---

###  Stop Container and Verify

![Stop Container](Screenshot 2026-04-14 152253.png)

✔ Container successfully stopped and removed from list

---

###  Kernel Logs (Memory Monitoring)

![Kernel Logs](Screenshot 2026-04-14 151052.png)

 Shows:

* Memory usage tracking
* Soft limit exceeded
* Hard limit triggered

---

###  Logging Output

![Engine Log](Screenshot 2026-04-14 151114.png)

 Logs show:

* Container start
* Container stop

---

##🧹 Cleanup Proof

```bash
ps aux | grep cpu_hog
```

Only `grep` appears → confirms:
✔ No zombie processes
✔ Proper cleanup

---

##  Conclusion

The project successfully implements a container runtime with kernel-level memory monitoring. It demonstrates process management, kernel communication using IOCTL, and resource control using soft and hard limits. The system efficiently manages multiple containers while ensuring proper cleanup and monitoring.

---
