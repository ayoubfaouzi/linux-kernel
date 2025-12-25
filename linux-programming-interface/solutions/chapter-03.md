> When using the Linux-specific reboot() system call to reboot the system, the second argument, magic2, must be specified as one of a set of magic numbers (e.g., LINUX_REBOOT_MAGIC2). What is the significance of these numbers? (Converting them to hexadecimal provides a clue.)

From the `man` pages:

```
      This system call fails (with the error EINVAL) unless magic equals
      LINUX_REBOOT_MAGIC1 (that is, 0xfee1dead) and magic2 equals
      LINUX_REBOOT_MAGIC2 (that is, 0x28121969).  However, since Linux
      2.1.17 also LINUX_REBOOT_MAGIC2A (that is, 0x05121996) and since
      Linux 2.1.97 also LINUX_REBOOT_MAGIC2B (that is, 0x16041998) and
      since Linux 2.5.71 also LINUX_REBOOT_MAGIC2C (that is, 0x20112000)
      are permitted as values for magic2.  (The hexadecimal values of
      these constants are meaningful.)
```

So possible values of `magic2` are:

- LINUX_REBOOT_MAGIC2 = 0x28121969  // Birthday of Linus Torvalds 
- LINUX_REBOOT_MAGIC2A = 0x05121996 // Birthday of Patricia Miranda Torvalds (b. 5-Dec-1996)
- LINUX_REBOOT_MAGIC2B = 0x16041998 // Birthday of Daniela Yolanda Torvalds (b. 16-Apr-1998)
- LINUX_REBOOT_MAGIC2C = 0x20112000 // Birthday of Celeste Amanda Torvalds (b. 20-Nov-2000)


Because `reboot()` is a very destructive, non-syncing operation that erases the state of the system -- thus erasing the evidence of the bit-flip problem that would otherwise be exposed as a program error or panic -- Linux includes extra protections around its successful use.
