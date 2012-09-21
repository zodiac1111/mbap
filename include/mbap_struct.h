#ifndef MBAP_STRUCT_H
#define MBAP_STRUCT_H
//�Զ�������
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned char rsp_dat;

//******* modbus function code R=Read W=Write ***********
//(��ǰ��ʵ��0x03)
#define MB_FUN_R_COILS		0x01	//��ȡ��ɢ��Ȧ 1bit
#define MB_FUN_R_DISCRETE_INPUT 0x02	// 1 bit
#define MB_FUN_R_HOLD_REG	0x03	//�����16λ���ּĴ���
#define MB_FUN_R_INPUT_REG	0x04	//��ȡ���16λ����Ĵ���
#define MB_FUN_W_SINGLE_COIL	0x05
#define MB_FUN_W_SINGLE_REG	0x06	//д�����Ĵ��� 16 bit
#define MB_FUN_R_EXCEPTION_STATUS 0x07	//(Serial Line only)
#define MB_FUN_Diagnostics	0x08	//(Serial Line only)
#define MB_FUN_GETCOMM_EVENT_CNT (0x0B) //Get Comm Event Counter (Serial Line only)
#define MB_FUN_GETCOMM_EVENT_LOG (0x0C) //Get Comm Event Log (Serial Line only)
#define MB_FUN_W_MULTI_COILS	0x0f	//д����Ȧ
#define MB_FUN_W_MULTI_REG	0x10	//д���16λ�Ĵ���
//(0x11) Report Slave ID (Serial Line only)
//(0x14) Read File Record
//(0x15) Write File Record
//(0x16) Mask Write Register
//(0x17) Read/Write Multiple registers
//(0x18) Read FIFO Queue
//(0x2B) Encapsulated Interface Transport
//(0x2B/0x0D) CANopen General Reference Request and Response PDU
//(0x2B/0x0E) Read Device Identification

//******************* �쳣ֵ 1�ֽ� *******************
//#define ERR_ILLEGAL_FUN			0x01
const unsigned char ERR_ILLEGAL_FUN		=0x01;
const unsigned char ERR_ILLEGAL_DATA_ADDRESS	=0x02;
const unsigned char ERR_ILLEGAL_DATA_VALUE	=0x03;
const unsigned char ERR_SLAVE_DEVICE_FAILURE	=0x04;
const unsigned char ERR_SLAVE_DEVICE_BUSY	=0x06;
//�쳣ֵ�����ı�:
#define ERR_ILLEGAL_FUN_MSG		"Illegal Function Code"
#define ERR_ILLEGAL_DATA_ADDRESS_MSG	"Illegal Date Address"
#define ERR_ILLEGAL_DATA_VALUE_MSG	"Illegal Date Valve(s)"
#define ERR_SLAVE_DEVICE_FAILURE_MSG	"Slave Device Failure"
#define ERR_SLAVE_DEVICE_BUSY_MSG	"Slave Device Busy"
//����
#define BYTE_PER_REG 2 //modbusÿ���Ĵ���ռ2�ֽ�,���Ĵ���Ϊ16λ.

/*modbusӦ�ó���ͷ MBAP headerͷ�ṹ��
MODBUS Application Protocol */
struct mbap_head{
	u8 TransID_hi;//����ʶ���� 2�ֽ�
	u8 TransID_lo;
	u8 protocolhead_hi;//Э��ʶ���� 2�ֽ� modbus=0
	u8 protocolhead_lo;
	u8 len_hi;//���泤�� 2�ֽ�
	u8 len_lo;
	u8 unitindet;//��վ/װ��ʶ���� 1�ֽ�
}/*_attribute__ ((packed))*/;	//�� 2+2+2+1= 7�ֽ�

//modbus����Э�����ݵ�Ԫ MODBUS Request PDU( ��������ּĴ��� ����)
struct mb_read_req_pdu{
	u8 func_code; //������ 1�ֽ�
	u8 start_addr_hi; //�Ĵ�����ʼ��ַ 2�ֽ�
	u8 start_addr_lo;
	u8 reg_quantity_hi; //�Ĵ�����Ŀ 2�ֽ�
	u8 reg_quantity_lo;
}/*__attribute__ ((packed))*/;	//��1+2+2=5�ֽ�
//(����)���ص�Ԫ MODBUS Response PDU( ��������ּĴ��� ����)
struct mb_read_rsp_pdu{
	u8 func_code;//������ 1�ֽ�
	u8 byte_count;//�����ֽ���N 1�ֽ�
	//u8 data * n //n=N or N+1(���ܱ�8����ʱ,������Ȧ����)
};//��1+1+N�ֽ�

//modbus����Э�����ݵ�Ԫ( д������ּĴ��� ����)
struct mb_write_req_pdu{
	u8 func_code; //������ 1�ֽ�
	u8 start_addr_hi; //�Ĵ�����ʼ��ַ 2�ֽ�
	u8 start_addr_lo;
	u8 reg_quantity_hi; //�Ĵ�����Ŀ 2�ֽ�
	u8 reg_quantity_lo;
	u8 byte_count;//д�ֽ���
	/* N*2byte NΪ�Ĵ������� */
}/*__attribute__ ((packed))*/;	//��1+2+2+1+N*2=6+N*2�ֽ�
//��Ӧ��Ԫ ( д������ּĴ��� ����)
struct mb_write_rsp_pdu{
	u8 func_code;//������ 1�ֽ�
	u8 start_addr_hi; //�Ĵ�����ʼ��ַ 2�ֽ�
	u8 start_addr_lo;
	u8 reg_quantity_hi; //�Ĵ�����Ŀ 2�ֽ�
	u8 reg_quantity_lo;
};//��1+2+2�ֽ�

//�쳣 ���ص�Ԫ MODBUS Exception Response PDU ���й��ܸ�ʽ��һ��
struct mb_excep_rsp_pdu{
	u8 exception_func_code; //�쳣������ 1�ֽ�
	u8 exception_code;//�쳣ֵ 1�ֽ�
};//��1+1=2�ֽ�

/*
The equivalent request to this Modbus RTU example (һ��modbus/RTU����)
		 11 03 006B 0003 7687
in Modbus TCP is:(modbus/tcp ����)
  0001 0000 0006 11 03 006B 0003
����:
0001: Transaction Identifier (����ʶ����)
0000: Protocol Identifier(Э��ʶ����,modbus=0x0000)
0006: Message Length (6 bytes to follow) (�����ֽ���)
11: The Unit Identifier  (17 = 11 hex) (��վ/�豸ʶ����)
03: The Function Code (read Analog Output Holding Registers)(������,��)
006B: The Data Address of the first register requested.
		(40108-40001 = 107 =6B hex)(��ʾ�Ĵ�����ַ)
0003: The total number of registers requested.
		(read 3 registers 40108 to 40110)(�ܹ���ȡ�Ĵ�������,3��)

*/
/* �ο�:
	1. refer: modbus/TCP http://www.simplymodbus.ca/TCP.htm
	2. http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
	3. http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (����)
	4. http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
	5. http://www.modbus.org/specs.php (˵����)
	6. ��Modbus��͸������ ��? ���� ��8��
*/
#endif // MBAP_STRUCT_H
