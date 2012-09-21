/*
	mdap.cpp:ModBus Application Protocol
	libmbap.so
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
//调试选项:
//#define REG_DAT_DEBUG //强制设置结构体数据,查看寄存器数据(和网络传输)正确性
#define SHOW_INPUT_MSG //在终端显示接收到 消息(报文)
#define SHOW_SEND_MSG //在终端显示 发送的 消息(报文)
/***************************** 接口 ***********************/
extern "C" CProtocol *CreateCProto_Cmbap()
{
	return  new Cmbap;
}

Cmbap::Cmbap()
{
	printf(MB_PERFIX"construct class\n");
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
	u8 len=0;
	//TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
	u8 readbuf[260];
	bool normal_response=false;//指示响应 为 正常响应
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
	//1.	复制出MBAP头
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//验证2 mbap头验证
	if(this->verify_mbap(req_mbap) == false){
		return -2;
	}
	//2.	复制出请求 pdu
	memcpy(&req_pdu,&readbuf[0]+sizeof(req_mbap),sizeof(req_pdu));
#ifdef SHOW_INPUT_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);
	print_req_pdu(req_pdu);printf("\n");
#endif
	//验证3 pdu验证:功能码,寄存器地址/数量
	bool verify_req= verify_req_pdu(req_pdu,excep_rsp_pdu);
	if(verify_req==false){
		//异常返回(构造异常报文)
		normal_response=false;
		memcpy(&rsp_mbap,&req_mbap,sizeof(req_mbap));
		rsp_mbap.len_lo=sizeof(rsp_mbap.unitindet)+sizeof(excep_rsp_pdu);
		//发送
		this->send_response(normal_response);
		return -3;
	}
	//正常返回
	normal_response=true;
	//构造数据
	map_dat2reg(this->reg_table,this-> m_meterData);//从 m_meterData 中复制数据到 reg-table
	//构造报文
	if(make_msg(this->req_mbap,this->req_pdu,this->ppdu_dat) != 0){
		printf(MB_ERR_PERFIX"make msg\n");
		return -4;
	}
	//发送
	this->send_response(normal_response);
	return 0;
}

/*	发送报文
	输入: normal_response(bool)是否为 正常返回报文
		成员变量 mbap,响应pdu,响应pdu-dat
	输出: m_transBuf(struct)	发送的报文.
*/
int Cmbap::send_response(const bool normal_response)
{
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(rsp_mbap);
	if(normal_response){
		this->print_rsp_pdu(rsp_pdu);
		this->print_pdu_dat(ppdu_dat,rsp_pdu.byte_count);
		printf("\n");
	}else{
		this->print_excep_rsp_pdu(excep_rsp_pdu);
		printf("\n");
	}
#endif
	//响应返回 send msg (to m_transBuf.m_transceiveBuf 数组)
	if(normal_response){//正常响应返回
		memcpy(&m_transBuf.m_transceiveBuf[0],	//mbap
		       &rsp_mbap,sizeof(rsp_mbap));//
		memcpy(&m_transBuf.m_transceiveBuf[0]+sizeof(rsp_mbap)//pdu
		       ,&rsp_pdu,sizeof(rsp_pdu));
		//pdu-dat
		memcpy(&m_transBuf.m_transceiveBuf[0]+sizeof(rsp_mbap)+sizeof(rsp_pdu)
		       ,ppdu_dat,rsp_pdu.byte_count);
		//trans count
		m_transBuf.m_transCount=sizeof(rsp_mbap) //mbap
				+sizeof(rsp_pdu) //pdu
				+(rsp_pdu.byte_count); //pdu-dat
	}else{//异常返回包
		//mbap
		memcpy(&m_transBuf.m_transceiveBuf[0],
		       &rsp_mbap,sizeof(rsp_mbap));//
		//excep-pdu
		memcpy(&m_transBuf.m_transceiveBuf[0]+sizeof(rsp_mbap)
		       ,&excep_rsp_pdu,sizeof(excep_rsp_pdu));
		//count
		m_transBuf.m_transCount=sizeof(rsp_mbap)+sizeof(excep_rsp_pdu);
	}
	return 0 ;
}

/*	构建返回的报文 TODO: 完善输入输出 减少耦合
	输入:	req_mbap (const)	请求的 mbap
		req_pdu  (const)	请求的 pdu
	输出:	ppdu_dat (*&u8)		返回报文数据 pdu-dat
	return:	0-成功 other-失败
  */
int Cmbap::make_msg( const struct mbap_head req_mbap
		     ,const struct mb_req_pdu req_pdu
		     ,u8 ppdu_dat[])
{
	int i; //构造前的准备工作
	int start_addr=req_pdu.start_addr_hi*256+req_pdu.start_addr_lo;
	int reg_quantity=req_pdu.reg_quantity_hi*256+req_pdu.reg_quantity_lo;
	rsp_pdu.byte_count=reg_quantity*BYTE_PER_REG;
	//构建mbap pdu 和pdu-dat 并准备发送
	//mbap
	rsp_mbap.TransID_hi=req_mbap.TransID_hi;//序列高
	rsp_mbap.TransID_lo=req_mbap.TransID_lo;//序列低
	rsp_mbap.protocolhead_hi=req_mbap.protocolhead_hi;//协议高
	rsp_mbap.protocolhead_lo=req_mbap.protocolhead_lo;//协议低
	rsp_mbap.len_hi=req_mbap.len_hi;//长度高(应该为0)
	rsp_mbap.len_lo=sizeof(rsp_mbap.unitindet) //长度低= 1byt的从站地址
			+sizeof(rsp_pdu) // + 2byte的 funcode+datlen
			+rsp_pdu.byte_count; // + dat len(byte)
	rsp_mbap.unitindet=req_mbap.unitindet;
	//pdu
	rsp_pdu.func_code=req_pdu.func_code;
	rsp_pdu.byte_count=reg_quantity*BYTE_PER_REG;
	//pdu-dat
	for(i=0;i<reg_quantity;i++){
		//modbus16位寄存器传输顺序:高字节在前,低字节在后
		ppdu_dat[i*2+0]=(reg_table[start_addr+i] & 0xFF00)>>8;
		ppdu_dat[i*2+1]=(reg_table[start_addr+i] & 0x00FF);
	}
	return 0;
}

//	输入验证 1	:验证信息体(报文),整体长度
bool Cmbap::verify_msg(u8* m_recvBuf,const u8 len) const
{
	if(len<7){
		printf(MB_ERR_PERFIX"mbap msg len:%d is less than 7.\n",len);
		return false;
	}
	//报文合理的长度
	int req_msg_len=sizeof(struct mbap_head)+sizeof(struct mb_req_pdu);
	if(len!=req_msg_len){//报文长度检查
		printf(MB_ERR_PERFIX"reci len=%d,it should be %d.\n",len, req_msg_len);
		return false;
	}
	return true;
}

/*	输入验证 2
	验证mbap头的合法性 合法返回 true 否则返回 false
	ref: http://modbus.control.com/thread/1026162917
*/
bool Cmbap::verify_mbap(const struct mbap_head req_mbap) const
{
	//1. 序列号:
	//	从站:忽略前2个字节,仅仅复制 返回行了
	//	对于主站,则需要检查序列号是否时之前自己发送的的序列号.
	//2. 协议: 0x0000 为 modbus
	if(!(req_mbap.protocolhead_hi==0x00
	     && req_mbap.protocolhead_lo==0x00)){
		printf(MB_ERR_PERFIX"mbap protocol not modbus/tcp protocol.\n");
		return false;
	}
	//3. 长度:
	// verify 5th byte is zero (upper length byte = zero as length MUST be
	// less than 256) 验证长度
	if(req_mbap.len_hi != 0x00 ){
		printf(MB_ERR_PERFIX"mbap the length of msg is too long.\n");
		return false;
	}
	//4. 从站编号 [2,253]
	//verify 6th byte is between 2 to 253. min cmd length is unit id
	if(!(req_mbap.unitindet>=2 && req_mbap.unitindet<= 253)){
		printf(MB_ERR_PERFIX"slave address (%d) is out of range.\n"
		       ,req_mbap.unitindet);
		return false;
	}
	return true;
}
/*	输入验证 3 pdu单元,功能码,寄存器地址/数量验证
	验证 req_pdu
	输入:	req_pdu
	输出:	excep_rsp_pdu(不合法时设置 | 合法时不变)
	返回值	: true:验证通过	fasle:不合法
*/
bool Cmbap:: verify_req_pdu(const struct mb_req_pdu req_pdu,
			    struct mb_excep_rsp_pdu &excep_rsp_pdu)
{
	u8 errcode=0;int reg_quantity; int start_addr;int end_addr;
	//1. 功能码是否支持 is_funcode_supported(req_pdu.func_code)
	if(verify_funcode(req_pdu.func_code)==false){
		printf(MB_ERR_PERFIX ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",req_pdu.func_code);
		errcode=ERR_ILLEGAL_FUN;
		goto IS_LEGAL;
	}
	if(verify_reg_quantity(req_pdu,reg_quantity)==false){
		printf(MB_ERR_PERFIX ERR_ILLEGAL_DATA_VALUE_MSG);
		printf(" reg_quantity=0x%04X.\n",reg_quantity);
		errcode=ERR_ILLEGAL_DATA_VALUE;
		goto IS_LEGAL;
	}
	if(verify_reg_addr(req_pdu,start_addr,end_addr)==false){
		printf(MB_PERFIX"ERR: "ERR_ILLEGAL_DATA_ADDRESS_MSG);
		printf(" start_addr=0x%04X end_addr=0x%04X.\n",start_addr,end_addr);
		errcode=ERR_ILLEGAL_DATA_ADDRESS;
		goto IS_LEGAL;
	}
	return true;
IS_LEGAL:
	excep_rsp_pdu.exception_code=errcode;
	//异常功能码=功能码+0x80 参考标准
	excep_rsp_pdu.exception_func_code=req_pdu.func_code+0x80;
	return false;
}

/*	输入验证 3-1
	验证功能码
	判断功能码是否非法,未实现的功能也是不合法
	param : 功能码
	return: true:通过验证	false:不合法
*/
inline bool Cmbap::verify_funcode(const u8  funcode) const
{
	switch (funcode){
	case MB_FUN_R_HOLD_REG:
		return true;
		break;
		//	case MB_FUN_W_SINGLE_REG:
		//		return true;
		//		break;
		/* .add funcode here */
	default://其他功能码不被支持
		return false;
		break;
	}
	return false;
}
/*	输入验证 3-2 验证 寄存器数量 是否符合相应的功能码
	输入: req_pdu	输出:reg_quantity寄存器数量
*/
inline bool Cmbap::verify_reg_quantity(const struct mb_req_pdu req_pdu
				       ,int &reg_quantity)const
{
	bool ret=true;
	reg_quantity=req_pdu.reg_quantity_hi*255+req_pdu.reg_quantity_lo;
	switch(req_pdu.func_code){
	case MB_FUN_R_HOLD_REG:	//寄存器数目[0x0001,0x07D]
		if(reg_quantity<0x0001 || reg_quantity>0x007D)
			ret=false;
		break;
	default:
		ret=false;
	}
	return ret;
}
/*	输入验证 3-3 验证 寄存器 地址 是否符合相应的功能码
	输入: req_pdu 输出: start_addr 开始地址 end_addr 结束地址
*/
inline bool Cmbap::verify_reg_addr(const struct mb_req_pdu req_pdu,
				   int &start_addr,int &end_addr )const
{
	int reg_quantity=0;bool ret=true;
	reg_quantity=req_pdu.reg_quantity_hi*255+req_pdu.reg_quantity_lo;
	start_addr=req_pdu.start_addr_hi*255+req_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity;
	switch(req_pdu.func_code){
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
inline void Cmbap::dat2mbreg(u16 reg[2],const unsigned int dat32)
{
	reg[0]=(dat32 & 0x0000FFFF)>>0; //低2字节在前(这个顺序是modbus决定的)
	reg[1]=(dat32 & 0xFFFF0000)>>16;//高2字节在后
}
//将32位 in t型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[2],const signed int dat32)
{
	reg[0]=(dat32 & 0x0000FFFF)>>0; //低2字节在前(这个顺序是modbus决定的)
	reg[1]=(dat32 & 0xFFFF0000)>>16;//高2字节在后
}
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中
inline void Cmbap::dat2mbreg(u16 reg[2],const float float32)
{
	//IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=( *(int *)&float32 & 0x0000FFFF)>>0;
	reg[1]=( *(int *)&float32 & 0xFFFF0000)>>16;
}
//将16位 short 型数据 转化成为 1个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[1],const short dat16)
{
	reg[0]=dat16;
}
//将 2 个 8位 char 数据 合成成为 1个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg(u16 reg[1]
			     ,const char high_byte,const char low_byte)
{
	reg[0]= (high_byte<<8) + low_byte ;
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
inline void Cmbap::print_req_pdu(const struct mb_req_pdu req_pdu)const
{
	printf("(%02X|%02X %02X|%02X %02X)"
	       ,req_pdu.func_code
	       ,req_pdu.start_addr_hi
	       ,req_pdu.start_addr_lo
	       ,req_pdu.reg_quantity_hi
	       ,req_pdu.reg_quantity_lo);
}
//打印异常响应pdu
inline void Cmbap::print_excep_rsp_pdu(struct mb_excep_rsp_pdu excep_rsp_pdu)const
{
	printf("(ERR:%02X|%02X)"
	       ,excep_rsp_pdu.exception_func_code
	       ,excep_rsp_pdu.exception_code);
}
//打印响应pdu
inline void Cmbap::print_rsp_pdu(const struct mb_rsp_pdu rsp_pdu) const
{
	printf("(%02X|%02X)"
	       ,rsp_pdu.func_code
	       ,rsp_pdu.byte_count);
}
//打印响应pdu数据体
inline void Cmbap::print_pdu_dat( const u8 ppdu_dat[], u8 bytecount)const
{
	int i;
	printf("[");
	//fflush(stdout);
	for(i=0;i<bytecount;i++){
		printf("%02X ",ppdu_dat[i]);
	}
	printf("]");
}

/*	将一些必要的数据从 stMeter_Run_data 结构体中复制(映射)到
	reg_table 寄存器表以便读取这些数据
	输入:	struct stMeter_Run_data m_meterData[MAXMETER]
	输出:	u16  reg_table[0xFFFF]
*/
int Cmbap::map_dat2reg(u16  reg_table[0xFFFF]
		       ,/*const*/ stMeter_Run_data m_meterData[])
//由于 REG_DAT_DEBUG 需要修改结构成员来调试,所以不使用 const 限制
{
	int i;
	int addr;
#ifdef REG_DAT_DEBUG
	//folat
	m_meterData[0]. m_iTOU[0]=1234.56789;
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
	//for (i=0;i<MAXMETER;i++){
	for (i=0;i<1;i++){
		addr=(i<<8);//高字节表示表号,分辨各个不同的表,范围[0,MAXMETER]
		//低字节表示各种数据,modbus寄存器16位,所以int型占用两个寄存器
		/*0x0000*/	dat2mbreg(&reg_table[addr+0x00],m_meterData[i].Flag_Meter);
		/*0x0001*/
		/*0x0002*/	dat2mbreg(&reg_table[addr+0x02],m_meterData[i].Flag_TOU);
		/*0x0003*/
		/*0x0004*/	dat2mbreg(&reg_table[addr+0x04],m_meterData[i].FLag_TA);
		/*0x0005*/
		/*0x0006*/	dat2mbreg(&reg_table[addr+0x06],m_meterData[i].Flag_MN);
		/*0x0007*/
		/*0x0008*/	dat2mbreg(&reg_table[addr+0x08],m_meterData[i].Flag_MNT);
		/*0x0009*/
		/*0x000a*/	dat2mbreg(&reg_table[addr+0x0a],m_meterData[i].Flag_QR);
		/*0x000b*/
		/*0x000c*/	dat2mbreg(&reg_table[addr+0x0c],m_meterData[i].Flag_LastTOU_Collect);
		/*0x000d*/
		/*0x000e*/	dat2mbreg(&reg_table[addr+0x0e],m_meterData[i].Flag_LastTOU_Save);
		/*0x000f*/
		/*0x0010*/	dat2mbreg(&reg_table[addr+0x10],m_meterData[i].Flag_PB);
		/*0x0011*/
		/*0x0012*/	dat2mbreg(&reg_table[addr+0x12],m_meterData[i].m_iTOU[0]);
		/*0x0013*/
		/*0x0014*/	dat2mbreg(&reg_table[addr+0x14],m_meterData[i].m_iMaxN[0]);
		/*0x0015*/
		/*0x0016*/	dat2mbreg(&reg_table[addr+0x16],m_meterData[i].m_iMaxNT[0]);
		/*0x0017*/
		/*0x0018*/	dat2mbreg(&reg_table[addr+0x18],m_meterData[i].m_iTOU_lm[0]);
		/*0x0019*/
		/*0x001a*/	dat2mbreg(&reg_table[addr+0x1a],m_meterData[i].m_iQR_lm[0]);
		/*0x001b*/
		/*0x001c*/	dat2mbreg(&reg_table[addr+0x1c],m_meterData[i].m_iMaxN_lm[0]);
		/*0x001d*/
		/*0x001e*/	dat2mbreg(&reg_table[addr+0x1e],m_meterData[i].m_iMaxNT_lm[0]);
		/*0x001f*/
		/*0x0020*/	dat2mbreg(&reg_table[addr+0x20],m_meterData[i].m_iQR[0]);
		/*0x0021*/
		/*0x0022*/	dat2mbreg(&reg_table[addr+0x22],m_meterData[i].m_iP[0]);
		/*0x0023*/
		/*0x0024*/	dat2mbreg(&reg_table[addr+0x24],m_meterData[i].m_wU[0]);
		/*0x0025*/
		/*0x0026*/	dat2mbreg(&reg_table[addr+0x26],m_meterData[i].m_wQ[0]);
		/*0x0027*/
		/*0x0028*/	dat2mbreg(&reg_table[addr+0x28],m_meterData[i].m_wPF[0]);
		/*0x0029*/
		/*0x002a*/	dat2mbreg(&reg_table[addr+0x2a],m_meterData[i].m_wPBCount[0]);
		/*0x002b*/
		/*0x002c*/	dat2mbreg(&reg_table[addr+0x2c],m_meterData[i].m_iPBTotalTime[0]);
		/*0x002d*/
		/*0x002e*/	dat2mbreg(&reg_table[addr+0x2e],m_meterData[i].m_iPBstarttime[0]);
		/*0x002f*/
		/*0x0030*/	dat2mbreg(&reg_table[addr+0x30],m_meterData[i].m_iPBstoptime[0]);
		/*0x0031*/
		/*0x0032*/	dat2mbreg(&reg_table[addr+0x32],m_meterData[i].ratio);
		/*0x0033*/
		/*0x0034*/	dat2mbreg(&reg_table[addr+0x34],m_meterData[i].C_Value);
		/*0x0035*/
		/*0x0036*/	dat2mbreg(&reg_table[addr+0x36],m_meterData[i].L_Value);
		/*0x0037*/
		/*0x0038*/	dat2mbreg(&reg_table[addr+0x38],m_meterData[i].m_cNetflag);
		/*0x0039*/
		/*0x003a*/	dat2mbreg(&reg_table[addr+0x3a],m_meterData[i]. m_cNetflag_tmp);
		/*0x003b*/
		/*0x003c*/	dat2mbreg(&reg_table[addr+0x3c],m_meterData[i].m_cF);//short
		/*0x003d*/	dat2mbreg(&reg_table[addr+0x3d],m_meterData[i].m_Ue);
		/*0x003e*/	dat2mbreg(&reg_table[addr+0x3e],m_meterData[i].m_Ie);
		/*0x003f*/	dat2mbreg(&reg_table[addr+0x3f]
					  ,m_meterData[i].Flag_P3L4,m_meterData[i].FLAG_LP);
		/*0x0040*/	dat2mbreg(&reg_table[addr+0x40]
					  ,m_meterData[i].FLAG_TIME,m_meterData[i].m_cMtrnunmb);
		/*0x0041*/	dat2mbreg(&reg_table[addr+0x41]
					  ,m_meterData[i].m_cCommflag,m_meterData[i].m_comflag_first);
		/*0x0042*/	dat2mbreg(&reg_table[addr+0x42]
					  ,m_meterData[i].m_cADDR[0],m_meterData[i].m_cADDR[1]);
		/*0x0043*/	dat2mbreg(&reg_table[addr+0x43]
					  ,m_meterData[i].m_cADDR[2],m_meterData[i].m_cADDR[3]);
		/*0x0044*/	dat2mbreg(&reg_table[addr+0x44]
					  ,m_meterData[i].m_cADDR[4],m_meterData[i].m_cADDR[5]);
		/*0x0045*/	dat2mbreg(&reg_table[addr+0x45]
					  ,m_meterData[i].m_com_Pwd[0],m_meterData[i].m_com_Pwd[1]);
		/*0x0046*/	dat2mbreg(&reg_table[addr+0x46]
					  ,m_meterData[i].m_com_Pwd[2],m_meterData[i].m_com_Pwd[3]);
		/*0x0047*/	dat2mbreg(&reg_table[addr+0x47]
					  ,m_meterData[i].m_cPortplan,m_meterData[i].m_cProt);
		/*0x0048*/	dat2mbreg(&reg_table[addr+0x48]
					  ,m_meterData[i].m_time[0],m_meterData[i].m_time[1]);
		/*0x0049*/	dat2mbreg(&reg_table[addr+0x49]
					  ,m_meterData[i].m_time[2],m_meterData[i].m_time[3]);
		/*0x004a*/	dat2mbreg(&reg_table[addr+0x4a]
					  ,m_meterData[i].m_time[4],m_meterData[i].m_time[5]);
		/*0x004b*/	dat2mbreg(&reg_table[addr+0x4b]
					  ,m_meterData[i].m_time[6],0);

	}
	//printf(MB_PERFIX"map dat to reg,ok.\n");
	return 0;
}
