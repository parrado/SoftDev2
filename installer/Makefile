EE_BIN = main.elf
EE_BIN_PACKED = ../SoftDev2Installer.elf
EE_BIN_STRIPPED = main-stripped.ELF


#IOP modules
EE_IOP_OBJS = FILEXIO_irx.o IOMANX_irx.o SIO2MAN_irx.o PADMAN_irx.o  DEV9_irx.o ATAD_irx.o HDD_irx.o PFS_irx.o 


EE_OBJS = main.o iop.o  pad.o install.o OPL_elf.o  ULE_elf.o MBR_xin.o gui.o textures.o  background_png.o background_success_png.o background_error_png.o logo_png.o cross_png.o circle_png.o square_png.o triangle_png.o $(EE_IOP_OBJS) 

EE_INCS := -I$(PS2SDK)/ports/include -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include  -I$(PS2SDK)/ports/include -I$(PS2SDK)/sbv/include -I$(GSKIT)/include -I$(GSKIT)/ee/dma/include -I$(GSKIT)/ee/gs/include -I$(GSKIT)/ee/toolkit/include 
EE_LDFLAGS := -L$(PS2SDK)/ports/lib -L$(PS2SDK)/ee/lib -L$(PS2DEV)/ee/ee/lib -L$(GSKIT)/lib
EE_LIBS =  -lpadx  -lpatches  -lc -lkernel -lfileXio -lhdd  -lgskit -ldmakit -lgskit_toolkit -L$(PS2SDK)/ports/lib -lpng  -lz
EE_GPVAL = -G0
EE_CFLAGS += -Os -mgpopt $(EE_GPVAL) -DNEWLIB_PORT_AWARE -D_EE

%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.S
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

%.o : %.s
	$(EE_AS) $(EE_ASFLAGS) $< -o $@


all:
	$(MAKE) -C ../launcher-hdd/payload
	../kelftool/kelftool.elf encrypt mbr ../launcher-hdd/payload/payload.bin INSTALL/MBR.XIN
	$(MAKE) $(EE_BIN_PACKED)


clean:
	$(MAKE) -C ../launcher-hdd/payload clean
	rm -f $(EE_BIN) $(EE_BIN_STRIPPED) $(EE_BIN_PACKED)$(EE_BIN_REL) $(EE_OBJS)  *_irx.c *_elf.c *_xin.c *_png.s *_ttf.s INSTALL/MBR.XIN
	

$(EE_BIN_STRIPPED): $(EE_BIN)
	$(EE_STRIP) -s -R .comment -R .gnu.version --strip-unneeded -o $@ $<
	
$(EE_BIN_PACKED): $(EE_BIN_STRIPPED)
	ps2-packer -v $< $@
SIO2MAN_irx.c: $(PS2SDK)/iop/irx/freesio2.irx
	bin2c $(PS2SDK)/iop/irx/freesio2.irx SIO2MAN_irx.c SIO2MAN_irx

PADMAN_irx.c: $(PS2SDK)/iop/irx/freepad.irx
	bin2c $(PS2SDK)/iop/irx/freepad.irx PADMAN_irx.c PADMAN_irx


ATAD_irx.c: $(PS2SDK)/iop/irx/ps2atad.irx
	bin2c $(PS2SDK)/iop/irx/ps2atad.irx ATAD_irx.c ATAD_irx

HDD_irx.c: $(PS2SDK)/iop/irx/ps2hdd-osd.irx
	bin2c $(PS2SDK)/iop/irx/ps2hdd-osd.irx HDD_irx.c HDD_irx

PFS_irx.c: $(PS2SDK)/iop/irx/ps2fs.irx
	bin2c $(PS2SDK)/iop/irx/ps2fs.irx PFS_irx.c PFS_irx

DEV9_irx.c: $(PS2SDK)/iop/irx/ps2dev9.irx
	bin2c $(PS2SDK)/iop/irx/ps2dev9.irx DEV9_irx.c DEV9_irx
	
IOMANX_irx.c: $(PS2SDK)/iop/irx/iomanX.irx
	bin2c $(PS2SDK)/iop/irx/iomanX.irx IOMANX_irx.c IOMANX_irx

FILEXIO_irx.c: $(PS2SDK)/iop/irx/fileXio.irx
	bin2c $(PS2SDK)/iop/irx/fileXio.irx FILEXIO_irx.c FILEXIO_irx

OPL_elf.c:
	bin2c INSTALL/OPNPS2LD.ELF OPL_elf.c OPL_elf

ULE_elf.c:
	bin2c INSTALL/ULE.ELF ULE_elf.c ULE_elf
	
MBR_xin.c:
	bin2c INSTALL/MBR.XIN MBR_xin.c MBR_xin
	
background_png.s: resources/bg.png
	bin2s $< $@ background_png
	
background_success_png.s: resources/bg_success.png
	bin2s $< $@ background_success_png	
	
background_error_png.s: resources/bg_error.png
	bin2s $< $@ background_error_png	
	
	
logo_png.s: resources/logo.png
	bin2s $< $@ logo_png	

cross_png.s: resources/button_cross.png
	bin2s $< $@ cross_png

circle_png.s: resources/button_circle.png
	bin2s $< $@ circle_png

triangle_png.s: resources/button_triangle.png
	bin2s $< $@ triangle_png

square_png.s: resources/button_square.png
	bin2s $< $@ square_png


include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
