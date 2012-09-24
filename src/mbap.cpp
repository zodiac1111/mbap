/*
	mdap.cpp:ModBus Application Protocol
	libmbap.so
	就实现了:0x06(读多个保持16位寄存器)
	TODO:	 0x10(写多个16位寄存器)
	功能码.
*/
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <malloc.h>
#include "define.h"
#include "loopbuf.h"
#include "sys_utl.h"
#include "log.h"
#include "mbap.h"


/***************************** 接口 ***********************/
extern "C" CProtocol *CreateCProto_Cmbap(void)
{
	//static_assert(sizeof(int) == 4, "Integer sizes expected to be 4");
	return  new Cmbap;
}

Cmbap::Cmbap()
{
	//	printf(MB_PERFIX"construct class\n");
	//	r[0]=&(m_meterData[0].Flag_Meter); //int
	//	r[4]=&(m_meterData[0].FLAG_LP); //char
	//	r[5]=&(m_meterData[0].FLAG_TIME); //char
	//	r[6]=&(m_meterData[0].m_iTOU[0]); //float
	//	//r[0]=&meterData[0].Flag_Meter;
	//	m_meterData[0].Flag_Meter=12345678;
	//	printf
	//	*(int *)r[0]=87654321;

}

Cmbap::~Cmbap()
{
	printf(MB_PERFIX"disconstruct class\n");
}

void Cmbap::m_BroadcastTime(void)// 广播
{
	return;
}

/*	TODO: 从站主动发送:
	当前没有被利用,可能的话可以用来从站主动上传某些报警信息,
	modbus串口传统形式是不支持从站主动上传的,这仅在modbus/tcp中定义.
*/
void Cmbap::SendProc(void)
{
	return;
}

/*	从站接收主站的接收函数:
	该模式是传统modbus/串口和modbus/tcp都有的.
	接收->分析->执行->返回 均由本函数实现
	分析/验证:由 verify_* 函数完成
	执行:	map_dat2reg
	构造报文: make_*_msg
	返回:		response_*
*/
//TODO:将各种逻辑功能相同的函数重载
int Cmbap::ReciProc(void)
{
	unsigned short  len=0;
	//TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
	u8 readbuf[260];
	bool verify_req=false;
	u8 errcode=0;
	//0. 接收
	len=get_num(&m_recvBuf);
	if(len==0){
		return 0;
	}
	copyfrom_buf(readbuf, &m_recvBuf, len);
	//验证1 报文验证
	if(verify_msg(readbuf,len)==false){
		printf(MB_PERFIX_ERR"reci illegal message\n");
		return -1;
	}
	syn_loopbuff_ptr(&m_recvBuf);//接收完成
	//复制出MBAP头
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//验证2 mbap头验证
	if(this->verify_mbap(req_mbap) == false){
		return -2;
	}
#ifdef DBG_SHOW_RECI_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);
	fflush(stdout);
#endif
	//验证:功能码,
	u8 funcode=readbuf[sizeof(req_mbap)];
	if(verify_funcode(funcode)==false){
		printf("(%02X|NaN)\n",funcode);
		printf(MB_PERFIX_ERR ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",funcode);
		this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
				     funcode,ERR_ILLEGAL_FUN);
		this->send_excep_response();
	}
	//根据功能码 判断 复制到不同的 请求pdu.
	switch (funcode){
	/******************************************************/
	case MB_FUN_R_HOLD_REG ://读多个保持寄存器
		memcpy(&read_req_pdu,&readbuf[0]+sizeof(req_mbap)
		       ,sizeof(read_req_pdu));
#ifdef DBG_SHOW_RECI_MSG
		print_req_pdu(read_req_pdu);
		printf("\n");
#endif
		//验证3 pdu验证:功能码,寄存器地址/数量(若异常,则构造异常pdu)
		verify_req= verify_req_pdu(read_req_pdu,errcode);
		if(verify_req==false){
			//验证错误
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,errcode);
			//发送
			this->send_excep_response();
			return -3;
		}
		//构造数据 m_meterData 中复制数据到 reg-table
		if(this->map_dat2reg(this->reg_table,this-> m_meterData,read_req_pdu) != 0 ){
			// 执行异常
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,
					     ERR_SLAVE_DEVICE_FAILURE);
			//发送
			this->send_excep_response();
			return -4;
		}
		//构造报文
		if(make_msg(this->req_mbap,this->read_req_pdu
			    ,this->rsp_mbap,this->read_rsp_pdu
			    ,this->ppdu_dat) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return -4;
		}
		//发送
		this->send_response(rsp_mbap,read_rsp_pdu,
				    &ppdu_dat[0],m_transBuf);
		break;
	/******************************************************/
	case MB_FUN_W_MULTI_REG://写多个寄存器 流程:参考文档[3].p31
		memcpy(&write_req_pdu,&readbuf[0]+sizeof(req_mbap)//pdu
		       ,sizeof(write_req_pdu));
#ifdef DBG_SHOW_RECI_MSG
		print_req_pdu(write_req_pdu);
		fflush(stdout);
#endif
		verify_req= verify_req_pdu(write_req_pdu,errcode);
		if(verify_req==false){
			//验证错误
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     write_req_pdu.func_code,errcode);
			//发送
			this->send_excep_response();
			return -3;
		}
		//pdu-dat(set reg as these values)
		memcpy(&ppdu_dat,
		       &readbuf[0]+sizeof(req_mbap)+sizeof(write_req_pdu),
		       write_req_pdu.byte_count);
#ifdef DBG_SHOW_RECI_MSG
		print_pdu_dat(ppdu_dat, write_req_pdu.byte_count);
		printf("\n");
#endif
		//写寄存器操作
		reg_table[0xfffb]=0x1234;
		reg_table[0xfffc]=0x5678;
		reg_table[0xfffd]=0x9abc;
		reg_table[0xfffe]=0xdef0;
		reg_table[0xffff]=u16((ppdu_dat[0]<<8)+ppdu_dat[1]);
		//构造报文
		if(make_msg(this->req_mbap,this->write_req_pdu
			    ,this->rsp_mbap,this->write_rsp_pdu) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return -4;
		}
		this->send_response(rsp_mbap,write_rsp_pdu,m_transBuf);
		break;
	default:
		return -4;
		break;
	}
	return 0;
}
//构造异常返回报文
int Cmbap::make_msg_excep(struct mbap_head &respond_mbap,
			  struct mb_excep_rsp_pdu &excep_respond_pdu,
			  u8 func_code, u8 exception_code) const
{
	memcpy(&respond_mbap,&req_mbap,sizeof(req_mbap));
	respond_mbap.len_lo=sizeof(respond_mbap.unitindet)
			+sizeof(excep_respond_pdu);
	excep_respond_pdu.exception_func_code=u8(func_code+0x80);
	excep_respond_pdu.exception_code=exception_code;
	return 0;
}

//发送异常返回报文
int Cmbap::send_excep_response(void)
{
#ifdef SHOW_SEND_ERR_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(this->rsp_mbap);
	this->print_rsp_pdu(this-> excep_rsp_pdu);
	printf("\n");
#endif
	//mbap
	memcpy(&m_transBuf.m_transceiveBuf[0]
	       ,&rsp_mbap,sizeof(rsp_mbap));//
	//excep-pdu
	memcpy(&m_transBuf.m_transceiveBuf[0]+sizeof(rsp_mbap)
	       ,&excep_rsp_pdu,sizeof(excep_rsp_pdu));
	//count
	m_transBuf.m_transCount=sizeof(rsp_mbap)+sizeof(excep_rsp_pdu);
	return 0;
}

/*	发送报文(功能码 0x06) 所有结构都是对于 0x06功能的
	输入: response_mbap  response_pdu  pdu_dat[255]
	输出: transBuf(struct)	发送的报文.
*/
int Cmbap::send_response(const struct mbap_head response_mbap
			 ,const struct mb_read_rsp_pdu response_pdu
			 ,const rsp_dat pdu_dat[255]
			 ,TransReceiveBuf &transBuf)const
{
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(response_mbap);
	this->print_rsp_pdu(response_pdu);
	this->print_pdu_dat(pdu_dat,response_pdu.byte_count);
	printf("\n");
#endif
	//响应返回 send msg (to m_transBuf.m_transceiveBuf 数组)
	memcpy(&transBuf.m_transceiveBuf[0]	//mbap
	       ,&response_mbap,sizeof(response_mbap));//
	memcpy(&transBuf.m_transceiveBuf[0]+sizeof(response_mbap)//pdu
	       ,&response_pdu,sizeof(response_pdu));
	//pdu-dat
	memcpy(&transBuf.m_transceiveBuf[0]
	       +sizeof(response_mbap)+sizeof(response_pdu)
	       ,pdu_dat,response_pdu.byte_count);
	//trans count
	transBuf.m_transCount=sizeof(response_mbap) //mbap
			+sizeof(response_pdu) //pdu
			+(response_pdu.byte_count); //pdu-dat
#ifdef DBG_send_response
	int i;
	printf("trancount=%d\n",transBuf.m_transCount);
	printf("sizeof=%d,%d\n",sizeof(response_mbap),sizeof(response_pdu));
	for(i=0;i<response_pdu.byte_count;i++){
		printf("%02X ",pdu_dat[i]);
	}
	printf("\n");
#endif
	return 0 ;
}
/*	发送报文(功能码 0x06) 所有结构都是对于 0x06功能的
	输入: response_mbap  response_pdu
	输出: transBuf(struct)	发送的报文.
*/
int Cmbap::send_response(const struct mbap_head response_mbap
			 ,const struct mb_write_rsp_pdu response_pdu
			 ,struct TransReceiveBuf &transBuf)const
{
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(response_mbap);
	this->print_rsp_pdu(response_pdu);
	printf("\n");
#endif
	//响应返回 send msg (to m_transBuf.m_transceiveBuf 数组)
	memcpy(&transBuf.m_transceiveBuf[0]	//mbap
	       ,&response_mbap,sizeof(response_mbap));//
	memcpy(&transBuf.m_transceiveBuf[0]+sizeof(response_mbap)//pdu
	       ,&response_pdu,sizeof(response_pdu));
	//trans count
	transBuf.m_transCount=sizeof(response_mbap) //mbap
			+sizeof(response_pdu); //pdu
//	printf("%02X %02X %02X %02X %02X %02X %02X cnt=%d \n",
//	       transBuf.m_transceiveBuf[0],
//	       transBuf.m_transceiveBuf[1],
//	       transBuf.m_transceiveBuf[2],
//	       transBuf.m_transceiveBuf[3],
//	       transBuf.m_transceiveBuf[4],
//	       transBuf.m_transceiveBuf[5],
//	       transBuf.m_transceiveBuf[6],transBuf.m_transCount);
	return 0 ;
}

/*	构建响应 0x06 的返回的报文
	输入:	request_mbap (const)	请求的 mbap
		request_pdu  (const)	请求的 pdu
	输出:	respond_mbap(&mbap_head)响应的 mbap
		respond_pdu(&mbap_head)	响应的 (正常)pdu
		pdu_dat (u8[255])	返回报文数据 pdu-dat
	return:	0-成功 other-失败
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_read_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_read_rsp_pdu &respond_pdu
		     ,u8 pdu_dat[255])const
{
	int i; //构造前的准备工作
	int start_addr=request_pdu.start_addr_hi*256+request_pdu.start_addr_lo;
	int reg_quantity=request_pdu.reg_quantity_hi*256+request_pdu.reg_quantity_lo;
	respond_pdu.byte_count=u8(reg_quantity*BYTE_PER_REG);
	//构建mbap pdu 和pdu-dat 并准备发送
	//mbap
	respond_mbap.TransID_hi=request_mbap.TransID_hi;//序列高
	respond_mbap.TransID_lo=request_mbap.TransID_lo;//序列低
	respond_mbap.protocolhead_hi=request_mbap.protocolhead_hi;//协议高
	respond_mbap.protocolhead_lo=request_mbap.protocolhead_lo;//协议低
	respond_mbap.len_hi=request_mbap.len_hi;//长度高(应该为0)
	respond_mbap.len_lo=u8(sizeof(respond_mbap.unitindet) //长度低= 1byt的从站地址
			       +sizeof(respond_pdu) // + 2byte的 funcode+datlen
			       +respond_pdu.byte_count); // + dat len(byte)
	respond_mbap.unitindet=request_mbap.unitindet;
	//pdu
	respond_pdu.func_code=request_pdu.func_code;
	respond_pdu.byte_count=u8(reg_quantity*BYTE_PER_REG);
	//pdu-dat
	for(i=0;i<reg_quantity;i++){
		//modbus16位寄存器传输顺序:高字节在前,低字节在后
		pdu_dat[i*2+0]=u8((reg_table[start_addr+i] & 0xFF00)>>8);
		pdu_dat[i*2+1]=u8((reg_table[start_addr+i] & 0x00FF));
#ifdef READ_DATE_PAND_DBG
		printf("%04X = %02X ,%02X\n",reg_table[start_addr+i],pdu_dat[i*2+0],pdu_dat[i*2+1]);
#endif
	}
//	printf("reg %02X ,%02X\n",pdu_dat[0],pdu_dat[1]);
//	printf("reg %02X ,%02X\n",pdu_dat[2],pdu_dat[3]);
	return 0;
}
/*	构建响应 0x10 的返回的报文
	输入:	request_mbap (const)	请求的 mbap
		request_pdu  (const)	请求的 pdu
	输出:	respond_mbap(&mbap_head)响应的 mbap
		respond_pdu(&mbap_head)	响应的 (正常)pdu
	return:	0-成功 other-失败
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_write_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_write_rsp_pdu &respond_pdu)const
{
	//构建mbap  和pdu 并准备发送
	//mbap
	respond_mbap.TransID_hi=request_mbap.TransID_hi;//序列高
	respond_mbap.TransID_lo=request_mbap.TransID_lo;//序列低
	respond_mbap.protocolhead_hi=request_mbap.protocolhead_hi;//协议高
	respond_mbap.protocolhead_lo=request_mbap.protocolhead_lo;//协议低
	respond_mbap.len_hi=request_mbap.len_hi;//长度高(应该为0)
	respond_mbap.len_lo=u8(sizeof(respond_mbap.unitindet) //长度低= 1byt的从站地址
			       +sizeof(respond_pdu)); // + pdu
	respond_mbap.unitindet=request_mbap.unitindet;
	//pdu
	respond_pdu.func_code=request_pdu.func_code;
	respond_pdu.start_addr_hi=request_pdu.start_addr_hi;
	respond_pdu.start_addr_lo=request_pdu.start_addr_lo;
	respond_pdu.reg_quantity_hi=request_pdu.reg_quantity_hi;
	respond_pdu.reg_quantity_lo=request_pdu.reg_quantity_lo;
	return 0;
}

//	输入验证 1	:验证信息体(报文),整体长度(大于7+1即合理)
bool Cmbap::verify_msg(u8* recvBuf, unsigned short len) const
{
	if(len<(sizeof(struct mbap_head)+1 /*funcode*/)){
		printf(MB_PERFIX_ERR"mbap msg len:%d is less than 7.\n",len);
		return false;
	}
	return true;
}

/*	输入验证 2
	验证mbap头的合法性 合法返回 true 否则返回 false
	ref: http://modbus.control.com/thread/1026162917
*/
bool Cmbap::verify_mbap(const struct mbap_head request_mbap) const
{
	//1. 序列号:
	//	从站:忽略前2个字节,仅仅复制 返回行了
	//	对于主站,则需要检查序列号是否时之前自己发送的的序列号.
	//2. 协议: 0x0000 为 modbus
	if(!(request_mbap.protocolhead_hi==0x00
	     && request_mbap.protocolhead_lo==0x00)){
		printf(MB_PERFIX_ERR"mbap protocol not modbus/tcp protocol.\n");
		return false;
	}
	//3. 长度:
	// verify 5th byte is zero (upper length byte = zero as length MUST be
	// less than 256) 验证长度
	if(request_mbap.len_hi != 0x00 ){
		printf(MB_PERFIX_ERR"mbap the length of msg is too long.\n");
		return false;
	}
	//4. 从站编号 [2,253]
	//verify 6th byte is between 2 to 253. min cmd length is unit id
	if(!(request_mbap.unitindet>=2 && request_mbap.unitindet<= 253)){
		printf(MB_PERFIX_ERR"slave address (%d) is out of range.\n"
		       ,request_mbap.unitindet);
		return false;
	}
	return true;
}

/*	输入验证 3 pdu单元,寄存器地址/数量验证
	验证 req_pdu
	输入:	req_pdu
	输出:	excep_rsp_pdu(不合法时设置 | 合法时不变)
	返回值	: true:验证通过	fasle:不合法
*/
bool Cmbap:: verify_req_pdu(const struct mb_read_req_pdu request_pdu,
			    u8 &errcode) const
{
	int reg_quantity; int start_addr;int end_addr;
	if(verify_reg_quantity(request_pdu,reg_quantity)==false){
		printf("\n"MB_PERFIX_ERR ERR_ILLEGAL_DATA_VALUE_MSG);
		printf(" reg_quantity=0x%04X.\n",reg_quantity);
		errcode=ERR_ILLEGAL_DATA_VALUE;
		return false;
	}
	if(verify_reg_addr(request_pdu,start_addr,end_addr)==false){
		printf("\n"MB_PERFIX_ERR ERR_ILLEGAL_DATA_ADDRESS_MSG);
		printf(" start_addr=0x%04X end_addr=0x%04X.\n",start_addr,end_addr);
		errcode=ERR_ILLEGAL_DATA_ADDRESS;
		return false;
	}
	return true;
}
bool Cmbap:: verify_req_pdu(const struct mb_write_req_pdu request_pdu,
			    u8 &errcode) const
{
	int reg_quantity; int start_addr;int end_addr;
	if(verify_reg_quantity(request_pdu,reg_quantity)==false){
		printf("\n"MB_PERFIX_ERR ERR_ILLEGAL_DATA_VALUE_MSG);
		printf(" reg_quantity=0x%04X.\n",reg_quantity);
		errcode=ERR_ILLEGAL_DATA_VALUE;
		return false;
	}
	if(verify_reg_addr(request_pdu,start_addr,end_addr)==false){
		printf("\n"MB_PERFIX_ERR ERR_ILLEGAL_DATA_ADDRESS_MSG);
		printf(" start_addr=0x%04X end_addr=0x%04X.\n",start_addr,end_addr);
		errcode=ERR_ILLEGAL_DATA_ADDRESS;
		return false;
	}
	return true;
}


/*	输入验证 3-1
	验证功能码
	判断功能码是否非法,未实现的功能也是不合法
	param : 功能码
	return: true:通过验证	false:不合法
*/
bool Cmbap::verify_funcode(const u8  funcode) const
{
	switch (funcode){
	case MB_FUN_R_COILS:
		printf(MB_PERFIX_WARN
		       "Function code: 0x01 read coils maybe on need \n");
		return false;
		break;
	case MB_FUN_R_HOLD_REG:
		return true;
		break;
	case MB_FUN_W_MULTI_REG: //TODO write funciton
		//printf(MB_PERFIX_WARN"Function code: 0x10 is now debuging :-) \n");
		return true;
		break;
		/* .add funcode here */
	default://其他功能码不被支持
		break;
	}
	return false;
}
/*	输入验证 3-2 验证 寄存器数量 是否符合相应的功能码
	输入:	req_pdu
	输出:	reg_quantity寄存器数量
*/
bool Cmbap::verify_reg_quantity(const struct mb_read_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007D){
		return false;
	}
	return true;
}
bool Cmbap::verify_reg_quantity(const struct mb_write_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007B){
		return false;
	}
	if(request_pdu.byte_count != (reg_quantity<<1) ){
		printf("cnt=%d quan= %d",request_pdu.byte_count,reg_quantity);
		return false;
	}
	return true;
}
/*	输入验证 3-3 验证 寄存器 地址 是否符合相应的功能码
	输入:	req_pdu
	输出:	start_addr 开始地址 end_addr 结束地址
*/
bool Cmbap::verify_reg_addr(const struct mb_read_req_pdu request_pdu,
			    int &start_addr,int &end_addr )const
{
	int reg_quantity=0;
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	start_addr=request_pdu.start_addr_hi*255+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity;
	// 起始和结束地址[0x0000,0xFFFF]
	if(start_addr<0x0000 || start_addr >0xFFFF
			|| end_addr<0x0000 || end_addr >0xFFFF ){
		return false;
	}
	return true;
}
bool Cmbap::verify_reg_addr(const struct mb_write_req_pdu request_pdu,
			    int &start_addr,int &end_addr )const
{
	int reg_quantity=0;
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	start_addr=request_pdu.start_addr_hi*255+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity;
	// 起始和结束地址[0x0000,0xFFFF]
	if(start_addr<0x0000 || start_addr >0xFFFF
			|| end_addr<0x0000 || end_addr >0xFFFF ){
		return false;
	}

	return true;
}
/********************* 一系列数据格式转换函数 **************************/
//将32位 int 型数据 转化成为 2个 modbus寄存器(16位)的形式 的高16位
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const unsigned int dat32) const
{
	reg[0]=u16((dat32 & 0xFFFF0000)>>16);//高16bit
}
//将32位 int 型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const unsigned int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //低16bit
}
//将32位 in t型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const signed int dat32) const
{
	reg[0]=u16((dat32 & 0xFFFF0000)>>16);//高2字节在后
}
//将32位 in t型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const signed int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //低2字节在前(这个顺序是modbus决定的)
}
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中
//IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const float float32) const
{
	//IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=u16(( *(int *)&float32 & 0xFFFF0000)>>16);
}
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const float float32) const
{
	reg[0]=u16(( *(int *)&float32 & 0x0000FFFF)>>0);
}
//将16位 short 型数据 转化成为 1个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[1],const short dat16) const
{
	reg[0]=dat16;
}
//将 2 个 8位 char 数据 合成成为 1个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[1]
			     ,const char high_byte,const char low_byte) const
{
	reg[0]=u16( (high_byte<<8) + low_byte) ;
}

/************************ 一系列打印函数 *******************************/
//打印 mbap 头信息 请求和响应 格式相同
void Cmbap::print_mbap( const struct mbap_head mbap) const
{
	printf("{%02X %02X|%02X %02X|%02X %02X|%02X}"
	       ,mbap.TransID_hi,mbap.TransID_lo //事务处理
	       ,mbap.protocolhead_hi,mbap.protocolhead_lo //协议
	       ,mbap.len_hi,mbap.len_lo //长度
	       ,mbap.unitindet); //单元
}
//打印请求pdu
inline void Cmbap::print_req_pdu(const struct mb_read_req_pdu request_pdu)const
{
	printf("(%02X|%02X %02X|%02X %02X)"
	       ,request_pdu.func_code
	       ,request_pdu.start_addr_hi
	       ,request_pdu.start_addr_lo
	       ,request_pdu.reg_quantity_hi
	       ,request_pdu.reg_quantity_lo);
}
//打印请求pdu
inline void Cmbap::print_req_pdu(const struct mb_write_req_pdu request_pdu)const
{
	printf("(%02X|%02X %02X|%02X %02X|%02X)"
	       ,request_pdu.func_code
	       ,request_pdu.start_addr_hi
	       ,request_pdu.start_addr_lo
	       ,request_pdu.reg_quantity_hi
	       ,request_pdu.reg_quantity_lo
	       ,request_pdu.byte_count);
}
//打印异常响应pdu
inline void Cmbap::print_rsp_pdu(
		const struct mb_excep_rsp_pdu excep_respond_pdu)const
{
	printf("(ERR:%02X|%02X)"
	       ,excep_respond_pdu.exception_func_code
	       ,excep_respond_pdu.exception_code);
}
//打印正常响应pdu 0x06 x010
inline void Cmbap::print_rsp_pdu(const struct mb_read_rsp_pdu respond_pdu)
const
{
	printf("(%02X|%02X)"
	       ,respond_pdu.func_code
	       ,respond_pdu.byte_count);
}
inline void Cmbap::print_rsp_pdu(const struct mb_write_rsp_pdu respond_pdu)
const
{
	printf("(%02X|%02X %02X|%02X %02X)"
	       ,respond_pdu.func_code
	       ,respond_pdu.start_addr_hi
	       ,respond_pdu.start_addr_lo
	       ,respond_pdu.reg_quantity_hi
	       ,respond_pdu.reg_quantity_lo);
}
//打印响应pdu数据体
inline void Cmbap::print_pdu_dat( const u8 pdu_dat[], u8 bytecount)const
{
	int i;
	printf("[");
	//fflush(stdout);
	for(i=0;i<bytecount;i++){
		printf("%02X ",pdu_dat[i]);
	}
	printf("\b]");
}
int Cmbap::map_reg2dat(u16  reg_tbl[0xFFFF]
		       ,stMeter_Run_data meterData[]
		       ,const struct mb_write_req_pdu request_pdu) const
{
	int i;int addr;
	u16 start_addr=u16((request_pdu.start_addr_hi<<8)+request_pdu.start_addr_lo);
	unsigned char meter_no=u8((start_addr & 0xFF00)>>8);
	unsigned char sub_id=u8((start_addr & 0xFF));
	u16 end_addr=u16((request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo);
	unsigned char meter_no_e=u8((end_addr & 0xFF00)>>8);
	for(i=meter_no;i<meter_no_e;i++){ //循环 表号
		addr=(i<<8);
		switch(sub_id){ //每个表的 子寄存器地址
		case 0x00:dat2mbreg_lo16bit(&reg_tbl[addr+0x00],meterData[i].Flag_Meter);
			if((i<<8 & 0x00) ==end_addr) goto OK;
		case 0x01:dat2mbreg_hi16bit(&reg_tbl[addr+0x01],meterData[i].Flag_Meter);
			if((i<<8 & 0x01) ==end_addr) goto OK;
		}
	}
OK:
	return 0;
}
/*	将一些必要的数据从 stMeter_Run_data 结构体中复制(映射)到
	reg_table 寄存器表以便读取这些数据
	TODO: 目前主站请求一次,测复制全部变量到寄存器数组,应改进效率!
	输入:	struct stMeter_Run_data m_meterData[MAXMETER]
	输出:	u16  reg_table[0xFFFF]
*/
int Cmbap::map_dat2reg(u16  reg_tbl[0xFFFF]
		       ,stMeter_Run_data meterData[]
		       ,const struct mb_read_req_pdu request_pdu) const

//由于 DBG_REG_DAT 需要修改结构成员来调试,所以不使用 const 限制
{
#ifdef DBG_REG_DAT //测试各种数据类型
	//folat
	m_meterData[0]. m_iTOU[0]=1234.56789F;
	printf(MB_PERFIX"dat debug: get (18)0x0012(float)    = %f \n"
	       ,m_meterData[0]. m_iTOU[0]);
	//unsigned int
	m_meterData[0]. Flag_Meter=1234567890;
	printf(MB_PERFIX"dat debug: get (0)0x0000(int)       = %d (0x%08X)\n"
	       ,m_meterData[0]. Flag_Meter,m_meterData[0]. Flag_Meter);
	//unsigned short
	m_meterData[0].m_Ie=12345;
	printf(MB_PERFIX"dat debug: get (62)0x003e(shrot)    = %d (0x%04X)\n"
	       ,m_meterData[0].m_Ie,m_meterData[0].m_Ie);
	//unsigned char
	m_meterData[0].m_cPortplan=0xab; m_meterData[0].m_cProt=0xcd;
	printf(MB_PERFIX"dat debug: get (71)0x0047(char,char)= %d (0x%X),%d (0x%X) \n"
	       ,m_meterData[0].m_cPortplan ,m_meterData[0].m_cPortplan
	       ,m_meterData[0].m_cProt,m_meterData[0].m_cProt);
#endif
	int i;
	int addr;

#if 1
	//以下是低效版本:高效版本TODO
	for (i=0;i<MAXMETER;i++) {
	//for (i=0;i<1;i++){
		addr=(i<<8);//高字节表示表号,分辨各个不同的表,范围[0,MAXMETER]
		//低字节表示各种数据,modbus寄存器16位,所以int型占用两个寄存器
		/*0x0000*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x00],meterData[i].Flag_Meter);
		/*0x0001*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x01],meterData[i].Flag_Meter);
		/*0x0002*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x02],meterData[i].Flag_TOU);
		/*0x0003*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x03],meterData[i].Flag_TOU);
		/*0x0004*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x04],meterData[i].FLag_TA);
		/*0x0005*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x05],meterData[i].FLag_TA);
		/*0x0006*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x06],meterData[i].Flag_MN);
		/*0x0007*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x07],meterData[i].Flag_MN);
		/*0x0008*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x08],meterData[i].Flag_MNT);
		/*0x0009*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x09],meterData[i].Flag_MNT);
		/*0x000a*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x0a],meterData[i].Flag_QR);
		/*0x000b*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x0b],meterData[i].Flag_QR);
		/*0x000c*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x0c],meterData[i].Flag_LastTOU_Collect);
		/*0x000d*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x0d],meterData[i].Flag_LastTOU_Collect);
		/*0x000e*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x0e],meterData[i].Flag_LastTOU_Save);
		/*0x000f*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x0f],meterData[i].Flag_LastTOU_Save);
		/*0x0010*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x10],meterData[i].Flag_PB);
		/*0x0011*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x11],meterData[i].Flag_PB);
		/*0x0012*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x12],meterData[i].m_iTOU[0]);
		/*0x0013*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x13],meterData[i].m_iTOU[0]);
		/*0x0014*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x14],meterData[i].m_iMaxN[0]);
		/*0x0015*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x15],meterData[i].m_iMaxN[0]);
		/*0x0016*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x16],meterData[i].m_iMaxNT[0]);
		/*0x0017*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x17],meterData[i].m_iMaxNT[0]);
		/*0x0018*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x18],meterData[i].m_iTOU_lm[0]);
		/*0x0019*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x19],meterData[i].m_iTOU_lm[0]);
		/*0x001a*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x1a],meterData[i].m_iQR_lm[0]);
		/*0x001b*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x1b],meterData[i].m_iQR_lm[0]);
		/*0x001c*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x1c],meterData[i].m_iMaxN_lm[0]);
		/*0x001d*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x1d],meterData[i].m_iMaxN_lm[0]);
		/*0x001e*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x1e],meterData[i].m_iMaxNT_lm[0]);
		/*0x001f*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x1f],meterData[i].m_iMaxNT_lm[0]);
		/*0x0020*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x20],meterData[i].m_iQR[0]);
		/*0x0021*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x21],meterData[i].m_iQR[0]);
		/*0x0022*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x22],meterData[i].m_iP[0]);
		/*0x0023*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x23],meterData[i].m_iP[0]);
		/*0x0024*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x24],meterData[i].m_wU[0]);
		/*0x0025*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x25],meterData[i].m_wU[0]);
		/*0x0026*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x26],meterData[i].m_wQ[0]);
		/*0x0027*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x27],meterData[i].m_wQ[0]);
		/*0x0028*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x28],meterData[i].m_wPF[0]);
		/*0x0029*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x29],meterData[i].m_wPF[0]);
		/*0x002a*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x2a],meterData[i].m_wPBCount[0]);
		/*0x002b*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x2b],meterData[i].m_wPBCount[0]);
		/*0x002c*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x2c],meterData[i].m_iPBTotalTime[0]);
		/*0x002d*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x2d],meterData[i].m_iPBTotalTime[0]);
		/*0x002e*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x2e],meterData[i].m_iPBstarttime[0]);
		/*0x002f*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x2f],meterData[i].m_iPBstarttime[0]);
		/*0x0030*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x30],meterData[i].m_iPBstoptime[0]);
		/*0x0031*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x31],meterData[i].m_iPBstoptime[0]);
		/*0x0032*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x32],meterData[i].ratio);
		/*0x0033*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x33],meterData[i].ratio);
		/*0x0034*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x34],meterData[i].C_Value);
		/*0x0035*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x35],meterData[i].C_Value);
		/*0x0036*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x36],meterData[i].L_Value);
		/*0x0037*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x37],meterData[i].L_Value);
		/*0x0038*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x38],meterData[i].m_cNetflag);
		/*0x0039*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x39],meterData[i].m_cNetflag);
		/*0x003a*/	dat2mbreg_lo16bit(&reg_tbl[addr+0x3a],meterData[i]. m_cNetflag_tmp);
		/*0x003b*/	dat2mbreg_hi16bit(&reg_tbl[addr+0x3b],meterData[i]. m_cNetflag_tmp);
		/*0x003c*/	dat2mbreg(&reg_tbl[addr+0x3c],meterData[i].m_cF);//short
		/*0x003d*/	dat2mbreg(&reg_tbl[addr+0x3d],meterData[i].m_Ue);
		/*0x003e*/	dat2mbreg(&reg_tbl[addr+0x3e],meterData[i].m_Ie);
		/*0x003f*/	dat2mbreg(&reg_tbl[addr+0x3f]
					  ,meterData[i].Flag_P3L4,meterData[i].FLAG_LP);
		/*0x0040*/	dat2mbreg(&reg_tbl[addr+0x40]
					  ,meterData[i].FLAG_TIME,meterData[i].m_cMtrnunmb);
		/*0x0041*/	dat2mbreg(&reg_tbl[addr+0x41]
					  ,meterData[i].m_cCommflag,meterData[i].m_comflag_first);
		/*0x0042*/	dat2mbreg(&reg_tbl[addr+0x42]
					  ,meterData[i].m_cADDR[0],meterData[i].m_cADDR[1]);
		/*0x0043*/	dat2mbreg(&reg_tbl[addr+0x43]
					  ,meterData[i].m_cADDR[2],meterData[i].m_cADDR[3]);
		/*0x0044*/	dat2mbreg(&reg_tbl[addr+0x44]
					  ,meterData[i].m_cADDR[4],meterData[i].m_cADDR[5]);
		/*0x0045*/	dat2mbreg(&reg_tbl[addr+0x45]
					  ,meterData[i].m_com_Pwd[0],meterData[i].m_com_Pwd[1]);
		/*0x0046*/	dat2mbreg(&reg_tbl[addr+0x46]
					  ,meterData[i].m_com_Pwd[2],meterData[i].m_com_Pwd[3]);
		/*0x0047*/	dat2mbreg(&reg_tbl[addr+0x47]
					  ,meterData[i].m_cPortplan,meterData[i].m_cProt);
		/*0x0048*/	dat2mbreg(&reg_tbl[addr+0x48]
					  ,meterData[i].m_time[0],meterData[i].m_time[1]);
		/*0x0049*/	dat2mbreg(&reg_tbl[addr+0x49]
					  ,meterData[i].m_time[2],meterData[i].m_time[3]);
		/*0x004a*/	dat2mbreg(&reg_tbl[addr+0x4a]
					  ,meterData[i].m_time[4],meterData[i].m_time[5]);
		/*0x004b*/	dat2mbreg(&reg_tbl[addr+0x4b]
					  ,meterData[i].m_time[6],0);

	}

#endif
//	for(i=0;i<10;i++)
//		printf("%02X ",reg_tbl[i]);

	//printf(MB_PERFIX"map dat to reg,ok.\n");
	return 0;
}
/* 参考文档:
	1. refer: modbus/TCP http://www.simplymodbus.ca/TCP.htm
	2. http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
	3. http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (官网)
	4. http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
	5. http://www.modbus.org/specs.php (说明书)
	6. 从Modbus到透明就绪 华? 编著 第8章
*/
