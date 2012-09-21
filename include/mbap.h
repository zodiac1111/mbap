/*	filename: mbap.h -> libmbap.so
	modbus的寄存器为16位,且按先高字节 后低字节传输
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
extern "C" CProtocol *CreateCProto_Cmbap(void);
//libmbap 定义的错误消息
//extern stMeter_Run_data m_meterData[MAXMETER];
// 调试选项:
//#define REG_DAT_DEBUG //强制设置结构体数据,用于寄存器数据(和网络传输)调试
#define SHOW_RECI_MSG //在终端显示接收到 消息(报文)
#define SHOW_SEND_MSG //在终端显示 发送的 消息(报文)
#define SHOW_SEND_ERR_MSG //在终端显示 发送的 异常 消息(报文)
//mbap规约
class Cmbap :public CProtocol
{
public:
	Cmbap();
	~Cmbap();
	void SendProc(void);
	int ReciProc(void);
	void m_BroadcastTime(void);
	//int Init(struct stPortConfig *tmp_portcfg);
	/************************** 成员变量 ****************************/
private:
	//请求
	struct mbap_head req_mbap;//请求头
	struct mb_read_req_pdu read_req_pdu;//读请求体
	struct mb_write_req_pdu write_req_pdu;//写请求体
	//响应
	struct mbap_head rsp_mbap;//回应头
	struct mb_read_rsp_pdu read_rsp_pdu;//读响应体-头
	struct mb_write_rsp_pdu write_rsp_pdu;//写响应体-头
	rsp_dat ppdu_dat[255];//指向(响应体-数据部分)的指针
	struct mb_excep_rsp_pdu excep_rsp_pdu;//异常响应体-头
	//所有寄存器表 16位每个 共0xFFFF个
	u16 reg_table[0xFFFF];
	/************************** 成员函数 ****************************/
private://输入验证
	bool verify_msg(u8* m_recvBuf,unsigned short len) const;
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
	int make_msg_excep(struct mbap_head &respond_mbap,
			   mb_excep_rsp_pdu &excep_respond_pdu,
			   u8 func_code, u8 exception_code)const;
	//发送异常回复
	int send_excep_response(void);
	//发送正常回复,重载
	int send_response(const mbap_head response_mbap
			  ,const mb_read_rsp_pdu response_pdu
			  ,const rsp_dat pdu_dat[],
			  struct TransReceiveBuf &transBuf) const;
	int send_response(const struct mbap_head response_mbap
				 ,const mb_write_rsp_pdu response_pdu
				 ,struct TransReceiveBuf transBuf)const;
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
	void dat2mbreg(u16 reg[2],const unsigned int dat32) const;
	void dat2mbreg(u16 reg[2],const signed int  dat32) const;
	void dat2mbreg(u16 reg[2],const float float32) const;
	void dat2mbreg(u16 reg[1],const short dat16) const;
	void dat2mbreg(u16 reg[1],const char high_byte,const char low_byte) const;
#ifdef REG_DAT_DEBUG
	int map_dat2reg(u16 reg_tbl[0xFFFF],stMeter_Run_data meterData[]) const;
#else
	int map_dat2reg(u16 reg_tbl[0xFFFF],const stMeter_Run_data meterData[])const;
#endif
};
#endif //__MBAP_H__
