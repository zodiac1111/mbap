使用说明 {#mainpage}
=======
##生成/查看帮助文档

在x86结构Linux操作系统下使用文档生成软件`doxygen`生成软件帮助手册:

	doxygen

在`html`目录下得到`html/index.html`文件.使用浏览器打开.

##编译

在x86结构Linux操作系统下使用交叉编译工具链`arm-linux-gcc`编译.需要make工具集.
在本软件根目录下执行:

	make rootdir=<DIR>

其中 `<DIR>` 表示hl3104源代码根目录.其中应包含 `<DIR>/include` 头文件目录和 `<DIR>/lib` arm版本的 `libsys_utl.so` 库文件.

当前`rootdir`默认的路径为`/home/lee/test/hl3104`.也可以在`Makefile`文件中手动修改.

完成后将在`lib`目录下得到一个`libmbap.so`库文件.

##安装

###上传规约文件
将`libmbap.so`库文件通过ftp软件或者其他方式上传到采集终端的`/mnt/nor/lib`目录下.

###配置规约
修改终端`/mnt/nor/conf/protocol_config.txt`文件.在其末尾添加一行:

	mbap 1 mbap Cproto_Cmbap

###修改终端配置
将终端地址设置为**255**.

###重启终端
重启终端令配置生效.

##其他说明
仅部分实现了协议所规定的0x03功能码,
仅实现保持寄存器的读操作,其他例如写输入寄存器,线圈等均没有实现.

对于float类型的传输,参考mbap(顺序)和IEEE-754 float 标准格式.

make-reg-map是辅助快速生成OPC服务器的配置文件.OPC服务器的读保存寄存器地址是从400001开始的.
范围为4000001~465536,对应即协议寄存器地址0x0000~0xFFFF.其中4为前缀.如果不是使用OPC服务器中转的可以忽略.

###关于终端地址
modbus/TCP 终端(从站)地址应设置为255,因为本协议不依靠终端地址寻址,而是通过IP地址寻址.所以终端地址这个量理论上是无用的.但是为了在同时具有modbus/TCP和modbus/串口的网络中不至于与串口的终端地址冲突.所以使用255为终端地址.基于串口的modbus总线协议应当忽略终端地址位255的报文.另,在仅有modbus/TCP的网络中.从站地址为0也是可以被接受的.但是为了以后扩展的考虑,本规约库实现均**必须且只能**指定终端(从站)地址为**255**.

##分类说明
* 现场安装调试人员查看:`doc/README.site.md`.
* 规约库(终端)开发人员查看: `doc/README.protocol-dev.md`.
* 主站规约开发人员查看: `doc/README.master-dev.md`.

##参考文档:
1. modbus/TCP <http://www.simplymodbus.ca/TCP.htm>
2. 基于TCP/IP的modbus报文 <http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf>
3. modbus/TCP 应用层协议: <http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf> (官网)
4. modbus/TCP 报文实现官方指导手册 <http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf>
5. modbus官方说明书: <http://www.modbus.org/specs.php> 
6. 从Modbus到透明就绪  第8章
7. IEEE 754 float 格式,在线转换 <http://babbage.cs.qc.cuny.edu/IEEE-754/> 或者 <http://www.binaryconvert.com/convert_float.html>
8. 维基百科:<http://en.wikipedia.org/wiki/Modbus>
9. 维基百科中文: <http://zh.wikipedia.org/wiki/Modbus>


