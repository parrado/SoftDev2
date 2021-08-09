LOADADDR  = 0x100000
EE_TARGET=payload
EE_BIN = $(EE_TARGET).elf
EE_BIN_RAW = $(EE_TARGET).bin
EE_BIN_STRIPPED = $(EE_TARGET)-stripped.elf

EE_BIN_RAW = $(EE_TARGET).bin
EE_OBJS = main.o  pad.o  timer.o elf_loader_elf.o $(EE_IOP_OBJS)
EE_IOP_OBJS = IOMANX_irx.o FILEXIO_irx.o SIO2MAN_irx.o PADMAN_irx.o DEV9_irx.o ATAD_irx.o HDD_irx.o PFS_irx.o


EE_LDFLAGS =  -L $(PS2SDK)/ee/lib -Wl,--gc-sections -Wl,-Ttext -Wl,$(LOADADDR)
EE_LIBS = -lelf-loader -lkernel -lpatches -lfileXio   -lpadx
EE_INCS= -I$(GSKIT)/include -I$(PS2SDK)/ports/include
EE_CFLAGS=-G0 -I $(PS2SDK)/ee/include -I $(PS2SDK)/common/include -D_EE -Os -Wall -ffunction-sections -fdata-sections -DNEWLIB_PORT_AWARE


all:
	$(MAKE) $(EE_BIN_PACKED) $(EE_BIN_RAW)
	../kelftool/kelftool.elf  encrypt mbr $(EE_BIN_RAW) ../../SoftDev2Installer/INSTALL/SYSTEM/MBR.XLF
	

clean:
	$(MAKE) -C ../elf-loader/ clean	
	rm -f *.elf *.ELF *.irx *.o *.s *.bin DEV9_irx.c IOMANX_irx.c FILEXIO_irx.c SIO2MAN_irx.c PADMAN_irx.c ATAD_irx.c HDD_irx.c PFS_irx.c

DEV9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2c $(PS2SDK)/iop/irx/ps2dev9.irx DEV9_irx.c DEV9_irx

IOMANX_irx.c: $(PS2SDK)/iop/irx/iomanX.irx
	bin2c $(PS2SDK)/iop/irx/iomanX.irx IOMANX_irx.c IOMANX_irx

FILEXIO_irx.c: $(PS2SDK)/iop/irx/fileXio.irx
	bin2c $(PS2SDK)/iop/irx/fileXio.irx FILEXIO_irx.c FILEXIO_irx

SIO2MAN_irx.c: $(PS2SDK)/iop/irx/freesio2.irx
	bin2c $(PS2SDK)/iop/irx/freesio2.irx SIO2MAN_irx.c SIO2MAN_irx

PADMAN_irx.c: $(PS2SDK)/iop/irx/freepad.irx
	bin2c $(PS2SDK)/iop/irx/freepad.irx PADMAN_irx.c PADMAN_irx


ATAD_irx.c: $(PS2SDK)/iop/irx/ps2atad.irx
	bin2c $(PS2SDK)/iop/irx/ps2atad.irx ATAD_irx.c ATAD_irx

HDD_irx.c: $(PS2SDK)/iop/irx/ps2hdd.irx
	bin2c $(PS2SDK)/iop/irx/ps2hdd.irx HDD_irx.c HDD_irx

PFS_irx.c: $(PS2SDK)/iop/irx/ps2fs.irx
	bin2c $(PS2SDK)/iop/irx/ps2fs.irx PFS_irx.c PFS_irx

elf_loader_elf.s: 
	$(MAKE) -C ../elf-loader/
	bin2s ../elf-loader/loader.elf elf_loader_elf.s elf_loader_elf	




$(EE_BIN_STRIPPED): $(EE_BIN)
	$(EE_STRIP) -s -R .comment -R .gnu.version --strip-unneeded -o $@ $<

$(EE_BIN_RAW): $(EE_BIN_STRIPPED)
	$(EE_OBJCOPY)  -O binary -v $< $@ 
	


include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

