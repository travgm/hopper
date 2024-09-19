hopper
======

Just a quick hop in the target elf and it modifies the interperter

```
$ readelf -l camel | egrep 'interpreter'
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]

$ ./hopper camel ld-camel.so
ELF file is a 64-Bit Shared Object (DYN) file

Found PT_INTERP segment at 0x0000000000000318
Offset: 0x318
Size: 28
Flags (p_flags): 0x4 ( R )
  Read:    Yes
  Write:   No
  Execute: No
OLD 0x318:PT_INTERP = /lib64/ld-linux-x86-64.so.2
NEW 0x318:PT_INTERP = ld-camel.so

$ readelf -l camel | egrep 'interpreter'
      [Requesting program interpreter: ld-camel.so]
```

copyright
=========
Copyright 2024 Travis Montoya "travgm"
