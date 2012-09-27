/*	filename: mbap.h -> libmbap.so
	modbus的寄存器为16位,且按先高字节 后低字节传输
	对于多于16位的数据类型如 int float,后2字节在前.
	注意数据格式,寄存器格式,传输顺序,
缩略语:
	ADU Application Data Unit
	IP Internet Protocol
	MB MODBUS
	MBAP MODBUS Application Protocol
	PDU Protocol Data Unit
	TCP Transport Control Protocol
*/
#ifndef __MBAP_H__
#define __MBAP_H__
#include "protocol.h"
#include "mbap_struct.h"
//显示mbap库消息的前缀.方便在显示中查看该库相关的信息.
#define MB_PERFIX "[libmbap]"	//用于一般信息 如接收数据
#define MB_PERFIX_WARN "[libmbap]Warning:"//用于提示 调试或建议信息
#define MB_PERFIX_ERR "[libmbap]ERR:"	//用于提示 错误信息
//libmbap 定义的错误消息
//extern stMeter_Run_data m_meterData[MAXMETER];
// 调试选项:
#define DBG_SHOW_RECI_MSG//在终端显示接收到 消息(报文)
#define SHOW_SEND_MSG //在终端显示 发送的 消息(报文)
#define SHOW_SEND_ERR_MSG //在终端显示 发送的 异常 消息(报文)
//#define READ_DATE_PAND_DBG //显示填充到寄存器的值
//#define DBG_send_response
//mbap规约
#define WARN_IF(EXP)					\
     do{ if ((EXP))					\
	     fprintf(stderr, "Warning: " #EXP "\n"); }	\
     while(0)
//debug print
//#define DBG_REG_DAT(something) something
//#define DP(something) {}
extern "C" CProtocol *CreateCProto_Cmbap(void);
class Cmbap :public CProtocol
{
public:
	Cmbap();
	~Cmbap();
	int Init(struct stPortConfig *tmp_portcfg);
	void SendProc(void);
	int ReciProc(void);
	void m_BroadcastTime(void);
	/************************** 成员变量 ****************************/
private:
	u8 slave_ID;//modbus从站(终端)ID
	struct stSyspara *sysConfig;//系统参数
	//请求
	struct mbap_head req_mbap;//请求头
	struct mb_read_req_pdu read_req_pdu;//读请求体
	struct mb_write_req_pdu write_req_pdu;//写请求体
	//响应
	struct mbap_head rsp_mbap;//回应头
	struct mb_read_rsp_pdu read_rsp_pdu;//读响应体-头
	struct mb_write_rsp_pdu write_rsp_pdu;//写响应体-头
	rsp_dat ppdu_dat[256];//指向(响应体-数据部分)的指针
	struct mb_excep_rsp_pdu excep_rsp_pdu;//异常响应体-头
	//所有寄存器表 16位每个 共0xFFFF+1个
	u16 reg_table[0xFFFF+1];
	/************************** 成员函数 ****************************/
private://输入验证
	bool verify_msg(unsigned short len) const;
	bool verify_mbap(const mbap_head request_mbap) const;
	bool verify_req_pdu(const struct  mb_read_req_pdu request_pdu,
			    u8 &errcode)const;
	bool verify_req_pdu(const struct  mb_write_req_pdu request_pdu,
			    u8 &errcode)const;
	bool verify_funcode( u8 funcode)const;
	bool verify_reg_addr(const struct mb_read_req_pdu request_pdu
			     ,int &start_addr,int &end_addr)const;
	bool verify_reg_addr(const struct mb_write_req_pdu request_pdu
			     ,int &start_addr,int &end_addr)const;
	bool verify_reg_quantity(const struct mb_read_req_pdu request_pdu
				 ,int &reg_quantity)const;
	bool verify_reg_quantity(const struct mb_write_req_pdu request_pdu
				 ,int &reg_quantity)const;
	//构建返回报文 0x06
	int make_msg( const struct mbap_head request_mbap
		      ,const struct mb_read_req_pdu read_req_pdu
		      ,struct mbap_head &rsp_mbap
		      ,struct mb_read_rsp_pdu &respond_pdu
		      ,u8 pdu_dat[])const;
	//构建返回报文 0x10
	int make_msg( const struct mbap_head request_mbap
		      ,const struct mb_write_req_pdu read_req_pdu
		      ,struct mbap_head &rsp_mbap
		      ,struct mb_write_rsp_pdu  &respond_pdu)const;
	//构造异常返回报文
	int make_msg_excep(struct mbap_head &mbap,
			   mb_excep_rsp_pdu &excep_pdu,
			   u8 func_code, u8 exception_code)const;
	//发送正常回复,0x06 0x10
	int send_response(const mbap_head mbap
			  ,const mb_read_rsp_pdu pdu
			  ,const rsp_dat pdu_dat[],
			  struct TransReceiveBuf &transBuf) const;
	int send_response(const struct mbap_head mbap
				 ,const mb_write_rsp_pdu pdu
				 ,TransReceiveBuf &transBuf)const;
	//发送异常回复
	int send_response_excep(const struct mbap_head mbap,
				const struct mb_excep_rsp_pdu pdu,
				struct TransReceiveBuf &transBuf )const ;
private://各种打印:	mbap头, 请求pdu
	void print_mbap( const mbap_head mbap)const;// 0x06 adn 0x10
	void print_req_pdu(const mb_read_req_pdu request_pdu)const;//read请求
	void print_req_pdu(const mb_write_req_pdu request_pdu)const;//write x010
	//		返回响应1 / 响应2
	void print_rsp_pdu(const mb_read_rsp_pdu respond_pdu)const;//0x06
	void print_rsp_pdu(const mb_write_rsp_pdu respond_pdu)const;//0x10
	void print_rsp_pdu(const mb_excep_rsp_pdu excep_respond_pdu)const;//异常
	void print_pdu_dat(const u8 pdu_dat[],u8 bytecount)const;//0x06/0x10数据体
private://实用函数 将各种类型转换成为 16位modbus寄存器类型
	void dat2mbreg_hi16bit(u16 reg[1],unsigned int &dat32, int dir) const;
	void dat2mbreg_lo16bit(u16 reg[1],unsigned int &dat32, int dir) const;
	void dat2mbreg_hi16bit(u16 reg[1],const signed int  dat32) const;
	void dat2mbreg_lo16bit(u16 reg[1],const signed int  dat32) const;
	void dat2mbreg_hi16bit(u16 reg[1],const float float32) const;
	void dat2mbreg_lo16bit(u16 reg[1],const float float32) const;
	void dat2mbreg(u16 reg[1],const short dat16) const;
	void dat2mbreg(u16 reg[1],const char high_byte,const char low_byte) const;
	//reg map
	int map_dat2reg(u16 reg[],stMeter_Run_data meter[]
			, const mb_read_req_pdu request_pdu)const;
	int map_reg2dat(u16 reg_tbl[]
			       ,stMeter_Run_data meter[]
			       ,const struct mb_write_req_pdu request_pdu) const;

};
#endif //__MBAP_H__
