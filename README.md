# SOFTDEV2 BY alexparrado (2021)


SoftDev2 automatically boots from __mbr partition in PS2 fat models with internal HDD.


# Installation instructions:


Run SoftDev2Installer.elf from your USB pendrive to install SoftDev MBR program (MBR.XIN) on your HDD. 


# Usage:


SoftDev2 should automatically boot from your HDD.

On boot SoftDev2 will launch OPL (hdd0:__sysconf/softdev2/OPNPS2LD.ELF) by default. In addition hot keys were added which will launch corresponding application if hot key is held during boot:

- CIRCLE button: uLaunchELF (hdd0:__sysconf/softdev2/ULE.ELF).
- TRIANGLE button: OSD ROM browser (rom0:OSDSYS), FreeHDBoot (hdd0:__system/osd/osdmain.elf) or HDD-OSD browser (hdd0:__system/osd/hosdsys.elf or hdd0:__system/osd100/hosdsys.elf) if any installed.

# Credits
ps2-browser launch feature is based on the FreeMCBoot 1.8 source code for OSDSYS patching, so credits go to Neme & jimmikaelkael.
