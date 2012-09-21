#ifndef MBAP_STRUCT_H
#define MBAP_STRUCT_H
//自定义数据
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned char rsp_dat;

//******* modbus function code R=Read W=Write ***********
//(当前仅实现0x03)
#define MB_FUN_R_COILS		0x01	//读取离散线圈 1bit
#define MB_FUN_R_DISCRETE_INPUT 0x02	// 1 bit
#define MB_FUN_R_HOLD_REG	0x03	//读多个16位保持寄存器
#define MB_FUN_R_INPUT_REG	0x04	//读取多个16位输入寄存器
#define MB_FUN_W_SINGLE_COIL	0x05
#define MB_FUN_W_SINGLE_REG	0x06	//写单个寄存器 16 bit
#define MB_FUN_R_EXCEPTION_STATUS 0x07	//(Serial Line only)
#define MB_FUN_Diagnostics	0x08	//(Serial Line only)
#define MB_FUN_GETCOMM_EVENT_CNT (0x0B) //Get Comm Event Counter (Serial Line only)
#define MB_FUN_GETCOMM_EVENT_LOG (0x0C) //Get Comm Event Log (Serial Line only)
#define MB_FUN_W_MULTI_COILS	0x0f	//写多线圈
#define MB_FUN_W_MULTI_REG	0x10	//写多个16位寄存器
//(0x11) Report Slave ID (Serial Line only)
//(0x14) Read File Record
//(0x15) Write File Record
//(0x16) Mask Write Register
//(0x17) Read/Write Multiple registers
//(0x18) Read FIFO Queue
//(0x2B) Encapsulated Interface Transport
//(0x2B/0x0D) CANopen General Reference Request and Response PDU
//(0x2B/0x0E) Read Device Identification

//******************* 异常值 1字节 *******************
//#define ERR_ILLEGAL_FUN			0x01
const unsigned char ERR_ILLEGAL_FUN		=0x01;
const unsigned char ERR_ILLEGAL_DATA_ADDRESS	=0x02;
const unsigned char ERR_ILLEGAL_DATA_VALUE	=0x03;
const unsigned char ERR_SLAVE_DEVICE_FAILURE	=0x04;
const unsigned char ERR_SLAVE_DEVICE_BUSY	=0x06;
//异常值描述文本:
#define ERR_ILLEGAL_FUN_MSG		"Illegal Function Code"
#define ERR_ILLEGAL_DATA_ADDRESS_MSG	"Illegal Date Address"
#define ERR_ILLEGAL_DATA_VALUE_MSG	"Illegal Date Valve(s)"
#define ERR_SLAVE_DEVICE_FAILURE_MSG	"Slave Device Failure"
#define ERR_SLAVE_DEVICE_BUSY_MSG	"Slave Device Busy"
//其他
#define BYTE_PER_REG 2 //modbus每个寄存器占2字节,即寄存器为16位.

/*modbus应用程序头 MBAP header头结构体
MODBUS Application Protocol */
struct mbap_head{
	u8 TransID_hi;//传输识别码 2字节
	u8 TransID_lo;
	u8 protocolhead_hi;//协议识别码 2字节 modbus=0
	u8 protocolhead_lo;
	u8 len_hi;//跟随长度 2字节
	u8 len_lo;
	u8 unitindet;//从站/装置识别码 1字节
}/*_attribute__ ((packed))*/;	//共 2+2+2+1= 7字节

//modbus请求协议数据单元 MODBUS Request PDU( 读多个保持寄存器 功能)
struct mb_read_req_pdu{
	u8 func_code; //功能码 1字节
	u8 start_addr_hi; //寄存器开始地址 2字节
	u8 start_addr_lo;
	u8 reg_quantity_hi; //寄存器数目 2字节
	u8 reg_quantity_lo;
}/*__attribute__ ((packed))*/;	//共1+2+2=5字节
//(正常)返回单元 MODBUS Response PDU( 读多个保持寄存器 功能)
struct mb_read_rsp_pdu{
	u8 func_code;//功能码 1字节
	u8 byte_count;//返回字节数N 1字节
	//u8 data * n //n=N or N+1(不能被8整除时,对于线圈操作)
};//共1+1+N字节

//modbus请求协议数据单元( 写多个保持寄存器 功能)
struct mb_write_req_pdu{
	u8 func_code; //功能码 1字节
	u8 start_addr_hi; //寄存器开始地址 2字节
	u8 start_addr_lo;
	u8 reg_quantity_hi; //寄存器数目 2字节
	u8 reg_quantity_lo;
	u8 byte_count;//写字节数
	/* N*2byte N为寄存器个数 */
}/*__attribute__ ((packed))*/;	//共1+2+2+1+N*2=6+N*2字节
//响应单元 ( 写多个保持寄存器 功能)
struct mb_write_rsp_pdu{
	u8 func_code;//功能码 1字节
	u8 start_addr_hi; //寄存器开始地址 2字节
	u8 start_addr_lo;
	u8 reg_quantity_hi; //寄存器数目 2字节
	u8 reg_quantity_lo;
};//共1+2+2字节

//异常 返回单元 MODBUS Exception Response PDU 所有功能格式都一样
struct mb_excep_rsp_pdu{
	u8 exception_func_code; //异常功能码 1字节
	u8 exception_code;//异常值 1字节
};//共1+1=2字节

/*
The equivalent request to this Modbus RTU example (一般modbus/RTU报文)
		 11 03 006B 0003 7687
in Modbus TCP is:(modbus/tcp 报文)
  0001 0000 0006 11 03 006B 0003
其中:
0001: Transaction Identifier (传输识别码)
0000: Protocol Identifier(协议识别码,modbus=0x0000)
0006: Message Length (6 bytes to follow) (后续字节数)
11: The Unit Identifier  (17 = 11 hex) (从站/设备识别码)
03: The Function Code (read Analog Output Holding Registers)(功能码,读)
006B: The Data Address of the first register requested.
		(40108-40001 = 107 =6B hex)(启示寄存器地址)
0003: The total number of registers requested.
		(read 3 registers 40108 to 40110)(总共读取寄存器数量,3个)

*/
/* 参考:
	1. refer: modbus/TCP http://www.simplymodbus.ca/TCP.htm
	2. http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
	3. http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (官网)
	4. http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
	5. http://www.modbus.org/specs.php (说明书)
	6. 从Modbus到透明就绪 华? 编著 第8章
*/
#endif // MBAP_STRUCT_H
