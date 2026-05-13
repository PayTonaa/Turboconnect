# ============================================================================
#  Name	 : build_help.mk
#  Part of  : TurboSound
# ============================================================================
#  Name	 : build_help.mk
#  Part of  : TurboSound
#
#  Description: This make file will build the application help file (.hlp)
# 
# ============================================================================

do_nothing :
	@rem do_nothing

# build the help from the MAKMAKE step so the header file generated
# will be found by cpp.exe when calculating the dependency information
# in the mmp makefiles.

MAKMAKE : TurboSound_0xE650F19F.hlp
TurboSound_0xE650F19F.hlp : TurboSound.xml TurboSound.cshlp Custom.xml
	cshlpcmp TurboSound.cshlp
ifeq (WINSCW,$(findstring WINSCW, $(PLATFORM)))
	md $(EPOCROOT)epoc32\$(PLATFORM)\c\resource\help
	copy TurboSound_0xE650F19F.hlp $(EPOCROOT)epoc32\$(PLATFORM)\c\resource\help
endif

BLD : do_nothing

CLEAN :
	del TurboSound_0xE650F19F.hlp
	del TurboSound_0xE650F19F.hlp.hrh

LIB : do_nothing

CLEANLIB : do_nothing

RESOURCE : do_nothing
		
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo TurboSound_0xE650F19F.hlp

FINAL : do_nothing
