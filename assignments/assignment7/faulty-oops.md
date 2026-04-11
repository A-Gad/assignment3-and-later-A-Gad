# faulty-oops analysis

## The Fault Type
`Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`
The kernel tried to write to address 0x0 — a null pointer. This is the root cause of the crash 

## CPU Exception Details

**EC = 0x25** — Data Abort at current Exception Level (kernel mode)
**WnR = 1** — it was a **write** operation that faulted, consistent with `echo > /dev/faulty`
**FSC = 0x05** — address 0x0 has no page table mapping at all

## Where the Fault Happened
looking at the pc(program counter) it points to the instruction that caused the crash
`pc : faulty_write+0x10/0x20 [faulty]`
this points to the function that caused the crash `faulty_write` inside module `[faulty]`
it is present at offset `+0x10` into the function, which is 32 bytes total `(/0x20)`

*to find the exact source line from the offset we use objdump*
`objdump -d faulty.ko | grep -A 20 "<faulty_write>"`

```
./output/host/bin/aarch64-linux-objdump -d output/build/ldd-fb28d7d40e71efb4007df9177cbfa4d9d3341208/misc-modules/faulty.ko | grep -A 20 "<faulty_write>"
0000000000000000 <faulty_write>:
   0:   d2800001        mov     x1, #0x0                        // #0   load null address into x1
   4:   d2800000        mov     x0, #0x0                        // #0   load 0 into x0 (return value)
   8:   d503233f        paciasp
   c:   d50323bf        autiasp
  10:   b900003f        str     wzr, [x1]                       // CRASH: write 0 to address in x1 (0x0)
  14:   d65f03c0        ret
  18:   d503201f        nop
  1c:   d503201f        nop
```
At offset **0x10**: `str wzr, [x1]` — store zero to the address held in x1,
which is 0x0 (null). This matches exactly what the oops reported.

## Call Trace

```
faulty_write+0x10/0x20 [faulty]   <- crash here
ksys_write+0x74/0x110
__arm64_sys_write+0x1c/0x30
```
Reading bottom to top: the `echo` write syscall was routed through the kernel
VFS layer down to `faulty_write`, where the crash occurred.