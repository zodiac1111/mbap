# ModBus Application Protocol #
仅部分实现了协议所规定的0x03功能码,
仅实现保持寄存器的读操作,其他例如写输入寄存器,线圈等均没有实现.

对于float类型的传输,参考mbap(顺序)和IEEE-754 float 标准格式

make-reg-map是辅助快速生成OPC服务器的配置文件.OPC服务器的读保存寄存器地址是从400001开始的.
范围为4000001~465536,对应即协议寄存器地址0x0000~0xFFFF.其中4为前缀.

###终端地址
modbus/TCP 终端(从站)地址应设置为255,因为本协议不依靠终端地址寻址,而是通过IP地址寻址.所以终端地址这个量理论上是无用的.但是为了在同时具有modbus/TCP和modbus/串口的网络中不至于与串口的终端地址冲突.所以使用255为终端地址.基于串口的modbus总线协议应当忽略终端地址位255的报文.另,在仅有modbus/TCP的网络中.从站地址为0也是可以被接受的.但是为了以后扩展的考虑,本规约库实现均**必须且只能**指定终端(从站)地址为**255**

#分类说明:
* 现场安装调试人员查看:README.site.md
* 规约库(终端)开发人员查看: README.protocol-dev.md
* 主站规约开发人员查看: README.master-dev.md
##参考文档:
1. modbus/TCP http://www.simplymodbus.ca/TCP.htm
2. 基于TCP/IP的modbus报文 http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
3. modbus/TCP 应用层协议:http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (官网)
4. modbus/TCP 报文实现官方指导手册 http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
5. modbus官方说明书: http://www.modbus.org/specs.php 
6. 从Modbus到透明就绪  第8章
7. IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
8. 维基百科:http://en.wikipedia.org/wiki/Modbus
9. 维基百科中文: http://zh.wikipedia.org/wiki/Modbus

##参考报文
<code>
1.[libmbap]<<< Reci form master:{32 C4|00 00|00 06|FF}(03|0E 20|00 08)
2.[libmbap]>>> Send  to  master:{32 C4|00 00|00 13|FF}(03|10)[00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80]
3.[libmbap]<<< Reci form master:{32 C5|00 00|00 06|FF}(03|0F 00|00 20)
4.[libmbap]>>> Send  to  master:{32 C5|00 00|00 43|FF}(03|40)[00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80]
5.[libmbap]<<< Reci form master:{32 C6|00 00|00 06|FF}(03|0F 20|00 08)
6.[libmbap]>>> Send  to  master:{32 C6|00 00|00 13|FF}(03|10)[00 00 BF 80 00 00 BF 80 00 00 BF 80 00 00 BF 80]
7.[libmbap]<<< Reci form master:{32 C7|00 00|00 06|FF}(03|10 00|00 20)
8.[libmbap]>>> Send  to  master:{32 C7|00 00|00 43|FF}(03|40)[C5 1F 43 82 C3 D7 43 82 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00]
9.[libmbap]<<< Reci form master:{32 C8|00 00|00 06|FF}(03|10 20|00 08)
10.[libmbap]>>> Send  to  master:{32 C8|00 00|00 13|FF}(03|10)[00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00]
</code>
