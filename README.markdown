# ModBus Application Protocol #
仅部分实现了协议所规定的0x03功能码,
仅实现保持寄存器的读操作,其他例如写输入寄存器,线圈等均没有实现.

对于float类型的传输,参考mbap(顺序)和IEEE-754 float 标准格式

make-reg-map是辅助快速生成OPC服务器的配置文件.OPC服务器的读保存寄存器地址是从400001开始的.
范围为4000001~465536,对应即协议寄存器地址0x0000~0xFFFF.其中4为前缀.



##参考:
1. refer: modbus/TCP http://www.simplymodbus.ca/TCP.htm
2. http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
3. http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (官网)
4. http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
5. http://www.modbus.org/specs.php (说明书)
6. 从Modbus到透明就绪  第8章
7. IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/

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
