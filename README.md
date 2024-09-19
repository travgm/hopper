hopper
======

Just a quick hop in the target elf and it modifies the interperter

```
$ readelf -l camel | egrep 'interpreter'
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]

$ ./hopper camel ld-camel.so
patching interpreter in camel with ld-camel.so

FOUND 0x318:PT_INTERP = /lib64/ld-linux-x86-64.so.2
SET 0x318:PT_INTERP = ld-camel.so

$ readelf -l camel | egrep 'interpreter'
      [Requesting program interpreter: ld-camel.so]
```

copyright
=========
Copyright 2024 Travis Montoya "travgm"
