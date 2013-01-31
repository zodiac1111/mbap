#定义hl3104根目录(rootdir) 
#1.需要其中的头文件参与编译
#2.需要其中的库文件.so参与连接.

#需要的头文件和库目录
rootdir= /home/lee/test/hl3104
#build目录,编译的所有东西将会在这里生成
builddir= lib
indir	= $(rootdir)/include
INCS	= -I./include -I$(indir)
LIB	= $(rootdir)/lib
LIBS	= -L$(LIB) -lsys_utl
CXX	= arm-linux-g++
NAME	= mbap
SOURCE	= src/mbap.cpp
#警告选项
wflags	=  -Wall -Wextra \
	-Wfloat-equal -Wshadow -Wconversion    \
	-Wmissing-declarations -Wundef -Wuninitialized -Wcast-align \
	-Wsign-compare -Wcast-qual -Wwrite-strings -Waggregate-return \
	-Woverloaded-virtual \
	-Wunreachable-code
# -Wpacked -Wpadded #用于最好的检测结构体内存对齐问题
# -Wunreachable-code #死代码
debugflag= -g
all:$(NAME)
$(NAME):$(SOURCE)
	[ -d "$(builddir)" ] || mkdir $(builddir) -p
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
	$^ -o $(builddir)/lib$@.so  $(LIBS) $(wflags) 
	cp $(builddir)/lib$@.so  $(LIB)/lib$@.so
#清理
clean:
	rm -rf $(LIB)/lib$(NAME).so
	rm -rf $(builddir)/lib$(NAME).so
#彻底清除
distclean:clean
	rm -rf $(builddir)
install:
	cp $(builddir)/lib$@.so  $(LIB)/lib$@.so
debug:$(SOURCE)
	$(CXX) $(LDFALGS) $(CXXFLAGS) -shared $(MARCO) $(INCS) \
	$^ -o $(LIB)/lib$(NAME).so $(LIBS) $(wflags) $(debugflag)

