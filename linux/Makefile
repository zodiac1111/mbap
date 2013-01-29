#定义hl3104根目录(rootdir) 
#1.需要其中的头文件参与编译
#2.需要其中的库文件.so参与连接.
rootdir =/home/lee/test/hl3104
indir=$(rootdir)/include
INCS    =       -I../include -I$(indir)
LIB	= $(rootdir)/lib
LIBS    = -L$(LIB) -lsys_utl
CXX	= arm-linux-g++
NAME    =       mbap
SOURCE  =       ../src/mbap.cpp
wflags  =  -Wall -Wfloat-equal -Wshadow -Wconversion    \
		-Wmissing-declarations -Wundef -Wuninitialized -Wcast-align \
		-Wsign-compare -Wcast-qual -Wwrite-strings -Waggregate-return \
		-Woverloaded-virtual
# -Wpacked -Wpadded 用于最好的检测结构体内存对齐问题
# -Wunreachable-code 死代码
debugflag= -g
all:$(NAME)
$(NAME):$(SOURCE)
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
	$^ -o lib$@.so  $(LIBS)
	cp lib$@.so  $(LIB)/lib$@.so
clean:
	rm -rf $(LIB)/libmbap.so
install:
debug:$(SOURCE)
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
	$^ -o $(LIB)/lib$(NAME).so $(LIBS) $(wflags) $(debugflag)

