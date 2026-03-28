# char_device — Linux Kernel Character Device Module

A simple Linux kernel module that creates a character device at `/dev/chardev`.
It acts as an in-memory notepad: user space can write text to it and read it back.

---

## Files

| File | Description |
|---|---|
| `char_device.c` | Kernel module source code |
| `Makefile` | Build system for compiling the module |
| `char_device.ko` | Compiled kernel object (generated after `make`) |

---

## How It Works

### Global State

| Variable | Type | Purpose |
|---|---|---|
| `dev_number` | `dev_t` | Major/minor number assigned by the kernel |
| `my_cdev` | `struct cdev` | Kernel's internal representation of the device |
| `my_class` | `struct class *` | Sysfs class, visible under `/sys/class/chardev_class/` |
| `my_device` | `struct device *` | Triggers udev to create `/dev/chardev` |
| `message[1024]` | `char[]` | In-memory buffer storing the last written data |
| `message_len` | `int` | Length of the current message in the buffer |

---

### File Operations

These functions are called when user space interacts with `/dev/chardev`:

| Function | Syscall | Description |
|---|---|---|
| `dev_open` | `open()` | Logs "device opened", returns success |
| `dev_release` | `close()` | Logs "device closed", returns success |
| `dev_read` | `read()` | Copies `message[]` from kernel → user space via `copy_to_user()` |
| `dev_write` | `write()` | Copies data from user space → `message[]` via `copy_from_user()` |

`copy_to_user` / `copy_from_user` are used instead of `memcpy` to safely cross the kernel/user-space boundary.

---

### Module Init — `chardev_init()`

Runs on `sudo insmod char_device.ko`. Four sequential steps:

1. **`alloc_chrdev_region`** — requests a unique major/minor number from the kernel.
2. **`cdev_init` + `cdev_add`** — registers the cdev and links it to the file operations.
3. **`class_create`** — creates `/sys/class/chardev_class/`.
4. **`device_create`** — notifies udev to create `/dev/chardev`.

Each step cleans up all previous steps on failure.

---

### Module Exit — `chardev_exit()`

Runs on `sudo rmmod char_device`. Tears down everything in reverse order:

1. `device_destroy` — removes `/dev/chardev`
2. `class_destroy` — removes the sysfs class
3. `cdev_del` — unregisters the character device
4. `unregister_chrdev_region` — releases the major/minor number

---

## Requirements

- Linux kernel headers matching your running kernel
- `build-essential` package

Install with:

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

---

## Build

```bash
cd char_device
make
```

This produces `char_device.ko` in the current directory.

To remove build artifacts:

```bash
make clean
```

---

## Usage

### Load the module

```bash
sudo insmod char_device.ko
```

### Verify it loaded

```bash
lsmod | grep char_device
```

Output:

```
char_device    16384  0
```

| Column | Meaning |
|---|---|
| `char_device` | module name |
| `16384` | size in bytes |
| `0` | number of processes currently using it |

### Check kernel log

```bash
dmesg | tail -5
```

Expected:

```
chardev: registered with major=240 minor=0
chardev: device created at /dev/chardev
```

### Write to the device

```bash
echo "hello kernel" | sudo tee /dev/chardev
```

### Read from the device

```bash
cat /dev/chardev
```

Expected output:

```
hello kernel
```

### Unload the module

```bash
sudo rmmod char_device
```

### Watch kernel logs in real time

```bash
sudo dmesg -w
```

Then perform reads/writes in another terminal to see the `printk` messages live.

---

## Module Metadata

| Field | Value |
|---|---|
| License | GPL |
| Author | beginner |
| Description | A simple character device driver |

---

## Follow-up: Possible Extensions

### 1. Multiple Device Instances

Create `/dev/chardev0`, `/dev/chardev1`, etc., each with its own independent buffer.
Requires allocating multiple minor numbers and managing an array of per-device state structs.
Good for learning how to scale a driver to handle multiple devices simultaneously.

---

### 2. IOCTL Commands

Add custom control commands beyond `read`/`write` using the `.unlocked_ioctl` field in `fops`.

Example commands:
- Clear the buffer
- Query the current message length
- Reverse the stored string

User space calls these via the `ioctl()` syscall, making the device controllable without reading or writing raw bytes.

---

### 3. Shared Buffer / Inter-Process Communication

Turn the device into a pipe-like IPC channel — one process writes, another reads.
Add a **wait queue** so the reader blocks until data is available instead of returning empty.
This teaches blocking I/O, `wait_event_interruptible`, and `wake_up`.

---

### 4. Circular Ring Buffer

Replace the fixed 1024-byte buffer with a ring buffer.
Producers keep writing and consumers keep reading without overwriting unread data.
This is how real devices like serial ports (`/dev/ttyS0`) manage streaming data.

---

### 5. Proc Filesystem Entry

Add a `/proc/chardev_stats` file that exposes runtime statistics:
- Total bytes written
- Total bytes read
- Number of open/close events
- Current buffer usage

Uses `proc_create()` and a `seq_file` interface — no changes to the main device logic needed.

---

### 6. Simple In-Kernel Key-Value Store

Let users write `key=value` pairs and retrieve values by key via `ioctl`.
Essentially a tiny in-kernel dictionary backed by a linked list or hash table.
A practical exercise in kernel memory management with `kmalloc` / `kfree`.

---

### 7. Encryption Layer

Encrypt data on `write` and decrypt on `read` using the kernel crypto API (`crypto_alloc_skcipher`).
A simpler starting point is XOR-based obfuscation before moving to AES.
Teaches how the kernel crypto subsystem works and how to chain it with device I/O.

---

### 8. Kernel Timer / Auto-Update Device

Use a `struct timer_list` to update the buffer automatically every N seconds
(e.g., a counter, system uptime, or simulated sensor value).
User space just polls `/dev/chardev` to get the latest value — no writes needed.
Introduces kernel timers and deferred work.

---

### Suggested Learning Progression

```
current state
    → IOCTL commands
        → multiple device instances
            → ring buffer
                → blocking I/O with wait queues
                    → kernel crypto layer
```

Each step builds directly on the previous one without requiring a full rewrite.

