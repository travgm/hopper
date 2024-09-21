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
Symbol Table '.dynsym' (STT_FUNC):
  0: 0000000000000000 __libc_start_main
  1: 0000000000000000 puts
  2: 0000000000000000 __cxa_finalize
Symbol Table '.symtab' (STT_FUNC):
  0: 0000000000001090 deregister_tm_clones
  1: 00000000000010c0 register_tm_clones
  2: 0000000000001100 __do_global_dtors_aux
  3: 0000000000001140 frame_dummy
  4: 0000000000000000 __libc_start_main@GLIBC_2.34
  5: 0000000000000000 puts@GLIBC_2.2.5
  6: 0000000000001174 _fini
  7: 0000000000001060 _start
  8: 0000000000001149 main
  9: 0000000000000000 __cxa_finalize@GLIBC_2.2.5
  10: 0000000000001000 _init
Patching Binary:
  OLD 0x318:PT_INTERP = /lib64/ld-linux-x86-64.so.2
  NEW 0x318:PT_INTERP = ld-camel.so

$ readelf -l camel | egrep 'interpreter'
      [Requesting program interpreter: ld-camel.so]
```

copyright
=========
Copyright 2024 Travis Montoya "travgm"
