INCS    =       -I../include -I../../../../include
LIB	= ../../../../lib
LIBS    = -L$(LIB) -lsys_utl
CXX	= arm-linux-g++
NAME    =       mbap
SOURCE  =       ../src/mbap.cpp
wflags  = -O -Wall -Wfloat-equal -Wshadow -Wconversion    \
		-Wmissing-declarations -Wundef -Wuninitialized -Wcast-align \
		-Wsign-compare -Wcast-qual -Wwrite-strings -Waggregate-return \
		-Woverloaded-virtual
# -Wpacked -Wpadded 用于最好的检测结构体内存对齐问题
# -Wunreachable-code 死代码
debugflag= -g 
all:$(NAME)
$(NAME):$(SOURCE)
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
		$^ -o $(LIB)/lib$@.so  $(LIBS)
clean:
	rm -rf $(LIB)/libmbap.so
install:
debug:$(SOURCE)
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
	     $^ -o $(LIB)/libmbap.so $(LIBS) $(wflags) $(debugflag)

