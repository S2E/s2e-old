LEVEL=../..

LIBRARYNAME=stp
BUILD_ARCHIVE=1

include $(LEVEL)/Makefile.common

STP_OBJS:=$(addprefix $(PROJ_OBJ_DIR)/../, $(addsuffix /$(BuildMode)/*.o, AST sat simplifier bitvec c_interface constantbv))

$(LibName.A): $(STP_OBJS)
ObjectsO+=$(STP_OBJS)

