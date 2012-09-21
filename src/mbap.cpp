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
*/
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
		printf(MB_ERR_PERFIX"reci illegal message\n");
		return -1;
	}
	syn_loopbuff_ptr(&m_recvBuf);//接收完成
	//复制出MBAP头
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//验证2 mbap头验证
	if(this->verify_mbap(req_mbap) == false){
		return -2;
	}
	//验证:功能码,
	u8 funcode=readbuf[sizeof(req_mbap)];
	if(verify_funcode(funcode)==false){
		printf(MB_PERFIX"<<< Reci form master:");
		print_mbap(req_mbap);
		printf("\n");
		printf(MB_ERR_PERFIX ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",funcode);
		this->make_excep_msg(rsp_mbap,excep_rsp_pdu,
				     funcode,ERR_ILLEGAL_FUN);
		this->send_excep_response();
	}
	//根据功能码 判断 复制到不同的 请求pdu.
	switch (funcode){
	case MB_FUN_R_HOLD_REG ://读多个保持寄存器
		memcpy(&read_req_pdu,&readbuf[0]+sizeof(req_mbap)
		       ,sizeof(read_req_pdu));
#ifdef SHOW_RECI_MSG
		printf(MB_PERFIX"<<< Reci form master:");
		print_mbap(req_mbap);
		print_req_pdu(read_req_pdu);
		printf("\n");
#endif
		//验证3 pdu验证:功能码,寄存器地址/数量(若异常,则构造异常pdu)
		verify_req= verify_req_pdu(read_req_pdu,errcode);
		if(verify_req==false){
			//异常返回(构造异常报文)
			this->make_excep_msg(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,errcode);
			//发送
			this->send_excep_response();
			return -3;
		}
		//正常返回
		//构造数据 m_meterData 中复制数据到 reg-table
		this->map_dat2reg(this->reg_table,this-> m_meterData);
		//构造报文
		if(make_read_msg(this->req_mbap,this->read_req_pdu
				 ,this->rsp_mbap,this->read_rsp_pdu
				 ,this->ppdu_dat) != 0){
			printf(MB_ERR_PERFIX"make msg\n");
			return -4;
		}
		//发送
		this->send_read_response();
		break;
	case MB_FUN_W_MULTI_REG://写多个寄存器
		memcpy(&write_req_pdu,&readbuf[0]+sizeof(req_mbap)
		       ,sizeof(write_req_pdu));
		memcpy(&ppdu_dat,
		       &readbuf[0]+sizeof(req_mbap)+sizeof(write_req_pdu),
		       write_req_pdu.byte_count);
		break;
	default:
		return -4;
		break;
	}
	return 0;
}
//构造异常返回报文
int Cmbap::make_excep_msg(struct mbap_head &respond_mbap,
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
	this->print_excep_rsp_pdu(this-> excep_rsp_pdu);
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
	输入:
	输出: m_transBuf(struct)	发送的报文.
*/
int Cmbap::send_read_response(void)
{
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(this->rsp_mbap);
	this->print_rsp_pdu(this->read_rsp_pdu);
	this->print_pdu_dat(this->ppdu_dat,this->read_rsp_pdu.byte_count);
	printf("\n");
#endif
	//响应返回 send msg (to m_transBuf.m_transceiveBuf 数组)
	memcpy(&m_transBuf.m_transceiveBuf[0]	//mbap
	       ,&rsp_mbap,sizeof(rsp_mbap));//
	memcpy(&m_transBuf.m_transceiveBuf[0]+sizeof(rsp_mbap)//pdu
	       ,&read_rsp_pdu,sizeof(read_rsp_pdu));
	//pdu-dat
	memcpy(&m_transBuf.m_transceiveBuf[0]
	       +sizeof(rsp_mbap)+sizeof(read_rsp_pdu)
	       ,ppdu_dat,read_rsp_pdu.byte_count);
	//trans count
	m_transBuf.m_transCount=sizeof(rsp_mbap) //mbap
			+sizeof(read_rsp_pdu) //pdu
			+(read_rsp_pdu.byte_count); //pdu-dat
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
int Cmbap::make_read_msg( const struct mbap_head request_mbap
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
	}
	return 0;
}

//	输入验证 1	:验证信息体(报文),整体长度(大于7+1即合理)
bool Cmbap::verify_msg(u8* recvBuf, unsigned short len) const
{
	if(len<(sizeof(struct mbap_head)+1 /*funcode*/)){
		printf(MB_ERR_PERFIX"mbap msg len:%d is less than 7.\n",len);
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
		printf(MB_ERR_PERFIX"mbap protocol not modbus/tcp protocol.\n");
		return false;
	}
	//3. 长度:
	// verify 5th byte is zero (upper length byte = zero as length MUST be
	// less than 256) 验证长度
	if(request_mbap.len_hi != 0x00 ){
		printf(MB_ERR_PERFIX"mbap the length of msg is too long.\n");
		return false;
	}
	//4. 从站编号 [2,253]
	//verify 6th byte is between 2 to 253. min cmd length is unit id
	if(!(request_mbap.unitindet>=2 && request_mbap.unitindet<= 253)){
		printf(MB_ERR_PERFIX"slave address (%d) is out of range.\n"
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
		printf(MB_ERR_PERFIX ERR_ILLEGAL_DATA_VALUE_MSG);
		printf(" reg_quantity=0x%04X.\n",reg_quantity);
		errcode=ERR_ILLEGAL_DATA_VALUE;
		return false;
	}
	if(verify_reg_addr(request_pdu,start_addr,end_addr)==false){
		printf(MB_PERFIX"ERR: "ERR_ILLEGAL_DATA_ADDRESS_MSG);
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
	case MB_FUN_R_HOLD_REG:
		return true;
		break;
	case MB_FUN_W_MULTI_REG: //TODO write funciton
		printf(MB_ERR_PERFIX"function come soon.\n");
		return false;
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
	bool ret=true;
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007D){
		ret=false;
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
	int reg_quantity=0;bool ret=true;
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	start_addr=request_pdu.start_addr_hi*255+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity;
	switch(request_pdu.func_code){
	case MB_FUN_R_HOLD_REG:
		// 起始和结束地址[0x0000,0xFFFF]
		if(start_addr<0x0000 || start_addr >0xFFFF
				|| end_addr<0x0000 || end_addr >0xFFFF ){
			ret= false;
		}
		break;
		//case other function code
	default:
		ret= false;
		break;
	}
	return ret;
}

/********************* 一系列数据格式转换函数 **************************/
//将32位 in t型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[2],const unsigned int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //低2字节在前(这个顺序是modbus决定的)
	reg[1]=u16((dat32 & 0xFFFF0000)>>16);//高2字节在后
}
//将32位 in t型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[2],const signed int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //低2字节在前(这个顺序是modbus决定的)
	reg[1]=u16((dat32 & 0xFFFF0000)>>16);//高2字节在后
}
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中
inline void Cmbap::dat2mbreg(u16 reg[2],const float float32) const
{
	//IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=u16(( *(int *)&float32 & 0x0000FFFF)>>0);
	reg[1]=u16(( *(int *)&float32 & 0xFFFF0000)>>16);
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
//打印异常响应pdu
inline void Cmbap::print_excep_rsp_pdu(
		const struct mb_excep_rsp_pdu excep_respond_pdu)const
{
	printf("(ERR:%02X|%02X)"
	       ,excep_respond_pdu.exception_func_code
	       ,excep_respond_pdu.exception_code);
}
//打印响应pdu
inline void Cmbap::print_rsp_pdu(const struct mb_read_rsp_pdu excep_respond_pdu)
const
{
	printf("(%02X|%02X)"
	       ,excep_respond_pdu.func_code
	       ,excep_respond_pdu.byte_count);
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

/*	将一些必要的数据从 stMeter_Run_data 结构体中复制(映射)到
	reg_table 寄存器表以便读取这些数据
	输入:	struct stMeter_Run_data m_meterData[MAXMETER]
	输出:	u16  reg_table[0xFFFF]
*/
int Cmbap::map_dat2reg(u16  reg_tbl[0xFFFF]
		       #ifdef REG_DAT_DEBUG
		       ,stMeter_Run_data meterData[]) const
#else
		       ,const stMeter_Run_data meterData[]) const
#endif
//由于 REG_DAT_DEBUG 需要修改结构成员来调试,所以不使用 const 限制
{
	int i;
	int addr;
#ifdef REG_DAT_DEBUG
	//folat
	m_meterData[0]. m_iTOU[0]=1234.56789F;
	printf(MB_PERFIX"dat debug: get (18)0x0012(float)    = %f \n"
	       ,m_meterData[0]. m_iTOU[0]);

	//printf("make a float from hex %x,sizeof this %x \n ",m_meterData[0].m_iTOU[0],
	//     sizeof(m_meterData[0].m_iTOU[0]	));
	//int
	m_meterData[0]. m_cNetflag_tmp=1234567890;
	printf(MB_PERFIX"dat debug: get (58)0x003a(int)      = %d \n"
	       ,m_meterData[0]. m_cNetflag_tmp);
	//short
	printf(MB_PERFIX"dat debug: get (62)0x003e(shrot)    = %d \n"
	       ,m_meterData[0].m_Ie);
	//char
	m_meterData[0].m_cPortplan=0xab; m_meterData[0].m_cProt=0xcd;
	printf(MB_PERFIX"dat debug: get (71)0x0047(char,char)= %X,%X \n"
	       ,m_meterData[0].m_cPortplan,m_meterData[0].m_cProt);
#endif
	for (i=0;i<MAXMETER;i++){
		//for (i=0;i<1;i++){
		addr=(i<<8);//高字节表示表号,分辨各个不同的表,范围[0,MAXMETER]
		//低字节表示各种数据,modbus寄存器16位,所以int型占用两个寄存器
		/*0x0000*/	dat2mbreg(&reg_tbl[addr+0x00],meterData[i].Flag_Meter);
		/*0x0001*/
		/*0x0002*/	dat2mbreg(&reg_tbl[addr+0x02],meterData[i].Flag_TOU);
		/*0x0003*/
		/*0x0004*/	dat2mbreg(&reg_tbl[addr+0x04],meterData[i].FLag_TA);
		/*0x0005*/
		/*0x0006*/	dat2mbreg(&reg_tbl[addr+0x06],meterData[i].Flag_MN);
		/*0x0007*/
		/*0x0008*/	dat2mbreg(&reg_tbl[addr+0x08],meterData[i].Flag_MNT);
		/*0x0009*/
		/*0x000a*/	dat2mbreg(&reg_tbl[addr+0x0a],meterData[i].Flag_QR);
		/*0x000b*/
		/*0x000c*/	dat2mbreg(&reg_tbl[addr+0x0c],meterData[i].Flag_LastTOU_Collect);
		/*0x000d*/
		/*0x000e*/	dat2mbreg(&reg_tbl[addr+0x0e],meterData[i].Flag_LastTOU_Save);
		/*0x000f*/
		/*0x0010*/	dat2mbreg(&reg_tbl[addr+0x10],meterData[i].Flag_PB);
		/*0x0011*/
		/*0x0012*/	dat2mbreg(&reg_tbl[addr+0x12],meterData[i].m_iTOU[0]);
		/*0x0013*/
		/*0x0014*/	dat2mbreg(&reg_tbl[addr+0x14],meterData[i].m_iMaxN[0]);
		/*0x0015*/
		/*0x0016*/	dat2mbreg(&reg_tbl[addr+0x16],meterData[i].m_iMaxNT[0]);
		/*0x0017*/
		/*0x0018*/	dat2mbreg(&reg_tbl[addr+0x18],meterData[i].m_iTOU_lm[0]);
		/*0x0019*/
		/*0x001a*/	dat2mbreg(&reg_tbl[addr+0x1a],meterData[i].m_iQR_lm[0]);
		/*0x001b*/
		/*0x001c*/	dat2mbreg(&reg_tbl[addr+0x1c],meterData[i].m_iMaxN_lm[0]);
		/*0x001d*/
		/*0x001e*/	dat2mbreg(&reg_tbl[addr+0x1e],meterData[i].m_iMaxNT_lm[0]);
		/*0x001f*/
		/*0x0020*/	dat2mbreg(&reg_tbl[addr+0x20],meterData[i].m_iQR[0]);
		/*0x0021*/
		/*0x0022*/	dat2mbreg(&reg_tbl[addr+0x22],meterData[i].m_iP[0]);
		/*0x0023*/
		/*0x0024*/	dat2mbreg(&reg_tbl[addr+0x24],meterData[i].m_wU[0]);
		/*0x0025*/
		/*0x0026*/	dat2mbreg(&reg_tbl[addr+0x26],meterData[i].m_wQ[0]);
		/*0x0027*/
		/*0x0028*/	dat2mbreg(&reg_tbl[addr+0x28],meterData[i].m_wPF[0]);
		/*0x0029*/
		/*0x002a*/	dat2mbreg(&reg_tbl[addr+0x2a],meterData[i].m_wPBCount[0]);
		/*0x002b*/
		/*0x002c*/	dat2mbreg(&reg_tbl[addr+0x2c],meterData[i].m_iPBTotalTime[0]);
		/*0x002d*/
		/*0x002e*/	dat2mbreg(&reg_tbl[addr+0x2e],meterData[i].m_iPBstarttime[0]);
		/*0x002f*/
		/*0x0030*/	dat2mbreg(&reg_tbl[addr+0x30],meterData[i].m_iPBstoptime[0]);
		/*0x0031*/
		/*0x0032*/	dat2mbreg(&reg_tbl[addr+0x32],meterData[i].ratio);
		/*0x0033*/
		/*0x0034*/	dat2mbreg(&reg_tbl[addr+0x34],meterData[i].C_Value);
		/*0x0035*/
		/*0x0036*/	dat2mbreg(&reg_tbl[addr+0x36],meterData[i].L_Value);
		/*0x0037*/
		/*0x0038*/	dat2mbreg(&reg_tbl[addr+0x38],meterData[i].m_cNetflag);
		/*0x0039*/
		/*0x003a*/	dat2mbreg(&reg_tbl[addr+0x3a],meterData[i]. m_cNetflag_tmp);
		/*0x003b*/
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
	//printf(MB_PERFIX"map dat to reg,ok.\n");
	return 0;
}
