SUBMAKE = $(MAKE) -C

LOADADDR  = 0x100000
EE_TARGET=mbr
EE_BIN = $(EE_TARGET).elf
EE_BIN_STRIPPED = $(EE_TARGET)-stripped.elf
EE_BIN_RAW = $(EE_TARGET).bin
EE_LZMA=lzma.a

EE_OBJS= main.o lzma-stub.o payload.o


EE_CFLAGS = -G0 -I $(PS2SDK)/ee/include -I $(PS2SDK)/common/include -I . -I common/lzma -D_EE -O3 -Wall -DDO_EXECPS2
EE_LDFLAGS =  -L lzma -L $(PS2SDK)/ee/lib -Wl,--gc-sections -nostartfiles -Wl,-Ttext -Wl,$(LOADADDR)
EE_LIBS = -lc -lkernel -lzma -ldebug



all: 	$(EE_LZMA) $(EE_BIN_RAW)			
	../kelftool/kelftool.elf  encrypt mbr $(EE_BIN_RAW) ../../SoftDev2Installer/INSTALL/SYSTEM/MBR.XLF	



$(EE_BIN_STRIPPED): $(EE_BIN)
	$(EE_STRIP) -s -R .comment -R .gnu.version --strip-unneeded -o $@ $<
	
	


$(EE_BIN_RAW): $(EE_BIN_STRIPPED)
	ee-objcopy -O binary -v $< $@ 
	
	
$(EE_LZMA):
	$(SUBMAKE) lzma


payload.s: 
	$(SUBMAKE) ../payload		
	bin2s ../payload/payload.bin payload.s payload
	
	

clean:
	rm -f *.o *.elf payload.s $(EE_TARGET).bin
	$(SUBMAKE) lzma clean
	$(SUBMAKE) ../payload clean






include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
