#主站开发人员说明

modbus/TCP协议文本说明书见官网:<http://www.modbus.org/specs.php>.
本规约实现的modbus/TCP的寄存器定义见doc/reg-map.xls

utility下面有简单的主站/从站仿真软件,支持win32和linux平台.可供调试之用.

示例报文:
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
