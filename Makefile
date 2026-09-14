#---------------------------------------------------------------------------------------------------------------------
# MOKU - Tactics of Go (Butano project makefile)
#---------------------------------------------------------------------------------------------------------------------
TARGET      	:=  moku
BUILD       	:=  build
LIBBUTANO   	?=  ../butano/butano
PYTHON      	:=  python3
SOURCES     	:=  src/go src/ai src/game src/game/scenes src/game/render
INCLUDES    	:=  include src
DATA        	:=
GRAPHICS    	:=  graphics
AUDIO       	:=  audio
AUDIOBACKEND	:=  maxmod
AUDIOTOOL		:=
DMGAUDIO    	:=
DMGAUDIOBACKEND	:=  default
ROMTITLE    	:=  MOKU
ROMCODE     	:=  BMKE
USERFLAGS   	:=  -DMOKU_GBA=1
USERCXXFLAGS	:=
USERASFLAGS 	:=
USERLDFLAGS 	:=
USERLIBDIRS 	:=
USERLIBS    	:=
DEFAULTLIBS 	:=
STACKTRACE		:=
USERBUILD   	:=
EXTTOOL     	:=

ifndef LIBBUTANOABS
	export LIBBUTANOABS	:=	$(realpath $(LIBBUTANO))
endif

include $(LIBBUTANOABS)/butano.mak

dist: all
	@tools/check_iwram.sh $(TARGET).elf
	@mkdir -p dist && cp $(TARGET).gba dist/moku.gba && $(DEVKITARM)/bin/arm-none-eabi-nm $(TARGET).elf > $(BUILD)/moku.sym && echo "dist/moku.gba ready"
