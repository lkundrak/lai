
**lai** (Lux ACPI Implementation) is a lightweight, portable, OS-independent implementation of the Advanced Configuration and Power Interface, aimed to provide OS developers with an easy way to use ACPI without having to write actual AML-parsing code.  

Instructions on porting lai to any OS are on [this page](https://omarrx024.github.io/docs/lai.html). For now, lai can do the following:
- Create the ACPI namespace, and manage it.
- Execute ACPI control methods, although many opcodes are missing.

The current state of lai is sufficient to run a [control method battery driver](https://github.com/omarrx024/lux/blob/master/kernel/acpi/battery.c) on VirtualBox and at least one test PC. More opcodes will be implemented as the project advances.

Contact me at omarx024@gmail.com. This project has been written for use with [lux](https://github.com/omarrx024/lux).

