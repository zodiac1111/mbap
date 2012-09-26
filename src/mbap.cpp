/*
	mdap.cpp:ModBus Application Protocol
	libmbap.so
	当前仅实现了:0x06(读多个保持16位寄存器)
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
#include "metershm.h"
/***************************** 接口 ***********************/
extern "C" CProtocol *CreateCProto_Cmbap(void)
{
	//static_assert(sizeof(int) == 4, "Integer sizes expected to be 4");
	return  new Cmbap;
}

Cmbap::Cmbap()
{

}

Cmbap::~Cmbap()
{
	printf(MB_PERFIX"disconstruct class\n");
}
//会被调用
int Cmbap::Init(struct stPortConfig *tmp_portcfg){
	if(tmp_portcfg->m_ertuaddr>>8 != 0x00){
		printf(MB_PERFIX_ERR"Slave ID len is more than 1 Byte"
		       ",please set less than 0xFF(255)\n");
		return -2;
	}
	this->slave_ID=(unsigned char)tmp_portcfg->m_ertuaddr;//终端编号
	printf(MB_PERFIX"slave_ID=%d\n",slave_ID);
	class METER_CShareMemory metershm;
	this->sysConfig = metershm.GetSysConfig();
	if(sysConfig==NULL){
		printf(MB_PERFIX_ERR"METER_CShareMemory metershm class\n");
		return -1;
	}
	//printf(MB_PERFIX"meter max quantity=%d\n",sysConfig->meter_num);
	return 0;
}

void Cmbap::m_BroadcastTime(void)// 广播(一般协议从站地址0x255为广播)
{
	return;
}

/*	TODO: 从站主动发送(待定):
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
	执行:	map_dat2reg(0x06) / map_reg2dat(0x10,未完成)
	构造报文: make_msg[_excep]
	返回:		response_*
*/
int Cmbap::ReciProc(void)
{
	//printf(MB_PERFIX" into ReciProc\n");
	unsigned short  len=0;
	u8 readbuf[260];//TCP MODBUS ADU = 253 bytes+MBAP (7 bytes) = 260 bytes
	bool verify_req=false;
	u8 errcode=0;int i;
	int start_addr;
	int reg_quantity;
	//0. 接收
	len=get_num(&m_recvBuf);
	if(len==0){
		return 0;
	}
	copyfrom_buf(readbuf, &m_recvBuf, len);
	//验证1 报文验证
	if( verify_msg(len)==false ){
		printf(MB_PERFIX_ERR"reci illegal message\n");
		return 0;
	}
	syn_loopbuff_ptr(&m_recvBuf);
	//接收完成,进行报文处理:检验,(执行)返回
	//复制出MBAP头
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//检查slave_ID判断是否是发送给本从站的
	if(req_mbap.unitindet != slave_ID ){
		printf("slave_ID=%d req_mbap.unitindet =%d",slave_ID,req_mbap.unitindet );
		return 0;
	}
	//验证 mbap头
	if(this->verify_mbap(req_mbap) == false){
		printf(MB_PERFIX_ERR"verify_mbap\n");
		return 0;
	}
#ifdef DBG_SHOW_RECI_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);fflush(stdout);//stdout为行缓存设备,强制刷新
#endif
	//验证 功能码,
	u8 funcode=readbuf[sizeof(req_mbap)];
	if(verify_funcode(funcode)==false){
		printf("(%02X|NaN)\n",funcode);
		printf(MB_PERFIX_ERR ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",funcode);
		make_msg_excep(rsp_mbap,excep_rsp_pdu,funcode,ERR_ILLEGAL_FUN);
		send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
	}
	//根据功能码 判断 复制到不同的 请求pdu.
	switch (funcode){
	/********************** 0x06 ******************************/
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
			this->send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
			return 0;
		}

		//构造数据 m_meterData 中复制数据到 reg-table
		if(this->map_dat2reg(this->reg_table,this-> m_meterData,read_req_pdu) != 0 ){

			// 执行异常
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,
					     ERR_SLAVE_DEVICE_FAILURE);
			//发送
			this->send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
			return 0;
		}
		//构造报文
		if(make_msg(this->req_mbap,this->read_req_pdu
			    ,this->rsp_mbap,this->read_rsp_pdu
			    ,this->ppdu_dat) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return 0;
		}
		//发送
		this->send_response(rsp_mbap,read_rsp_pdu,
				    &ppdu_dat[0],m_transBuf);
		break;
		/********************** 0x10 *****************************/
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
			this->send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
			return 0;
		}
		//pdu-dat(set reg as these values)
		memcpy(&ppdu_dat,
		       &readbuf[0]+sizeof(req_mbap)+sizeof(write_req_pdu),
		       write_req_pdu.byte_count);
#ifdef DBG_SHOW_RECI_MSG
		print_pdu_dat(ppdu_dat, write_req_pdu.byte_count);
		printf("\n");
#endif
		//写寄存器操作 (DEMO)
		start_addr=(write_req_pdu.start_addr_hi<<8)
				+write_req_pdu.start_addr_lo;
		reg_quantity=(write_req_pdu.reg_quantity_hi<<8)
				+write_req_pdu.reg_quantity_lo;
		for(i=0;i<reg_quantity;i++){
			reg_table[start_addr+i]=u16((ppdu_dat[i*2+0]<<8)+ppdu_dat[i*2+1]);
			printf("regtable=%X ;",reg_table[start_addr+i]);
		}
		map_reg2dat(reg_table,m_meterData,write_req_pdu);
		//构造报文
		if(make_msg(this->req_mbap,this->write_req_pdu
			    ,this->rsp_mbap,this->write_rsp_pdu) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return 0;
		}
		this->send_response(rsp_mbap,write_rsp_pdu,m_transBuf);
		break;
	default:
		return 0;
		break;
	}
	//printf(MB_PERFIX"out of ReciProc\n");
	return 0;
}

/*	发送报文(功能码 0x06) 所有结构都是对于 0x06功能的
	输入: response_mbap  response_pdu  pdu_dat[256]
	输出: transBuf(struct)	发送的报文.
*/
int Cmbap::send_response(const struct mbap_head mbap
			 ,const struct mb_read_rsp_pdu pdu
			 ,const rsp_dat pdu_dat[256]
			 ,TransReceiveBuf &transBuf)const
{
	//响应返回 send msg (to m_transBuf.m_transceiveBuf 数组)
	memcpy(&transBuf.m_transceiveBuf[0]	//mbap
	       ,&mbap,sizeof(mbap));//
	memcpy(&transBuf.m_transceiveBuf[0]+sizeof(mbap)//pdu
	       ,&pdu,sizeof(pdu));
	//pdu-dat
	memcpy(&transBuf.m_transceiveBuf[0]
	       +sizeof(mbap)+sizeof(pdu)
	       ,pdu_dat,pdu.byte_count);
	//trans count
	transBuf.m_transCount=sizeof(mbap) //mbap
			+sizeof(pdu) //pdu
			+(pdu.byte_count); //pdu-dat
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(mbap);
	this->print_rsp_pdu(pdu);
	this->print_pdu_dat(pdu_dat,pdu.byte_count);
	printf("\n");
#endif
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
/*	发送报文(功能码 0x10) 所有结构都是对于 0x10功能的
	输入: response_mbap  response_pdu
	输出: transBuf(struct)	发送的报文.
*/
int Cmbap::send_response(const struct mbap_head mbap
			 ,const struct mb_write_rsp_pdu pdu
			 ,struct TransReceiveBuf &transBuf)const
{
	memcpy(&transBuf.m_transceiveBuf[0]	//mbap
	       ,&mbap,sizeof(mbap));
	memcpy(&transBuf.m_transceiveBuf[0]+sizeof(mbap)//pdu
	       ,&pdu,sizeof(pdu));
	transBuf.m_transCount=sizeof(mbap)//count
			+sizeof(pdu);
#ifdef SHOW_SEND_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(mbap);
	this->print_rsp_pdu(pdu);
	printf("\n");
#endif
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
//发送异常返回报文
int Cmbap::send_response_excep(const struct mbap_head mbap,
			       const struct mb_excep_rsp_pdu pdu,
			       struct TransReceiveBuf &transBuf )const
{
	memcpy(&transBuf.m_transceiveBuf[0]//mbap
	       ,&mbap,sizeof(struct mbap_head));//
	memcpy(&transBuf.m_transceiveBuf[0]+sizeof(mbap)//excep-pdu
	       ,&pdu,sizeof(struct mb_excep_rsp_pdu));
	transBuf.m_transCount=sizeof(mbap)+sizeof(pdu);//count
#ifdef SHOW_SEND_ERR_MSG
	printf(MB_PERFIX">>> Send  to  master:");
	this->print_mbap(mbap);
	this->print_rsp_pdu(pdu);
	printf("\n");
#endif
	return 0;
}
/*	构建响应 0x06 的返回的报文
	输入:	request_mbap (const)	请求的 mbap
		request_pdu  (const)	请求的 pdu
	输出:	respond_mbap(&mbap_head)响应的 mbap
		respond_pdu(&mbap_head)	响应的 (正常)pdu
		pdu_dat (u8[256])	返回报文数据 pdu-dat
	return:	0-成功 other-失败
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_read_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_read_rsp_pdu &respond_pdu
		     ,u8 pdu_dat[256])const
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
//构造异常返回报文
int Cmbap::make_msg_excep(struct mbap_head &mbap,
			  struct mb_excep_rsp_pdu &excep_pdu,
			  u8 func_code, u8 exception_code) const
{
	memcpy(&mbap,&req_mbap,sizeof(req_mbap));
	mbap.len_lo=sizeof(mbap.unitindet)
			+sizeof(excep_pdu);
	excep_pdu.exception_func_code=u8(func_code+0x80);
	excep_pdu.exception_code=exception_code;
	return 0;
}
//	输入验证 1	:验证信息体(报文),整体长度(大于7+1即合理)
bool Cmbap::verify_msg( unsigned short len) const
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
/*	输入验证
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
	case MB_FUN_W_MULTI_REG:
		return false;
		break;
		/* .add other funcode here */
	default://其他功能码不被支持
		break;
	}
	return false;
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
		printf(" start_addr=0x%X end_addr=0x%X.\n",start_addr,end_addr);
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
		printf(" start_addr=0x%X end_addr=0x%X.\n",start_addr,end_addr);
		errcode=ERR_ILLEGAL_DATA_ADDRESS;
		return false;
	}
	return true;
}

/*	输入验证  验证 寄存器数量 是否符合相应的功能码
	输入:	req_pdu
	输出:	reg_quantity寄存器数量
*/
bool Cmbap::verify_reg_quantity(const struct mb_read_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=(request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007D){
		return false;
	}
	return true;
}
bool Cmbap::verify_reg_quantity(const struct mb_write_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=(request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007B){
		return false;
	}
	if(request_pdu.byte_count != (reg_quantity<<1) ){
		printf("cnt=%d quan= %d",request_pdu.byte_count,reg_quantity);
		return false;
	}
	return true;
}
/*	输入验证  验证 寄存器 地址 是否符合相应的功能码
	输入:	req_pdu
	输出:	start_addr 开始地址 end_addr 结束地址
*/
bool Cmbap::verify_reg_addr(const struct mb_read_req_pdu request_pdu,
			    int &start_addr,int &end_addr )const
{
	int reg_quantity=0;
	reg_quantity=(request_pdu.reg_quantity_hi << 8)+request_pdu.reg_quantity_lo;
	start_addr=(request_pdu.start_addr_hi << 8) +request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity-1;
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
	reg_quantity=(request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo;
	start_addr=(request_pdu.start_addr_hi<<8)+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity-1;
	// 起始和结束地址[0x0000,0xFFFF]
	if(start_addr<0x0000 || start_addr >0xFFFF
			|| end_addr<0x0000 || end_addr >0xFFFF ){
		return false;
	}
	return true;
}
/********************* 一系列数据格式转换函数 **************************/
//将32位 int 型数据 转化成为 2个 modbus寄存器(16位)的形式 的高16位
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1], unsigned int &dat32,int dir=0) const
{
	//	if(dir==0){
	reg[0]=u16((dat32 & 0xFFFF0000)>>16);//高16bit
	//	}else{
	//		dat32=(dat32 & 0x0000FFFF) | (reg[0]<<16);
	//		printf("dat32=%X reg=%X ",dat32,reg[0]);
	//	}
}
//将32位 int 型数据 转化成为 2个 modbus寄存器(16位)的形式
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1], unsigned int &dat32,int dir=0) const
{
	//	if(dir==0){
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //低16bit
	//	}else{
	//		dat32=(dat32 & 0xFFFF0000) | reg[0];
	//		printf("dat32=%X reg=%X ",dat32,reg[0]);
	//	}
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
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中(高16位)
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const float float32) const
{
	//IEEE 754 float 格式,在线转换 http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=u16(( *(int *)&float32 & 0xFFFF0000)>>16);
}
//将32位 float 型数据 转化保存在 2个 modbus 寄存器(16位)中(低16位)
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

/*	将一些必要的数据从 stMeter_Run_data 结构体中复制(映射)到
	reg_tbl 寄存器表以便读取这些数据
	输入:	struct stMeter_Run_data meter[MAXMETER]
	输出:	u16  reg[0xFFFF+1] 寄存器表
	TODO:	目前主站请求一次,复制全部变量到寄存器数组,稍显浪费.
		应改进效率,仅复制需要的结构体变量.
	NOTE:	modbus寄存器起始地址分清0开始还是1开始.
*/
int Cmbap::map_dat2reg(u16  reg[0xFFFF+1]
		       ,stMeter_Run_data meter[]
		       ,const struct mb_read_req_pdu request_pdu) const
{
	int i;int j;
	int base;u8 sub=0;// base(表序号)+sub(项序号)=地址
#if 1
	//printf("%d\n",sysConfig->meter_num);
	for (i=0;i<sysConfig->meter_num;i++) {//(复制所有变量)
		base=(i<<8);	//高字节表示表号,分辨各个不同的表,范围[0,MAXMETER]
		sub=0;				//子域,某个表的特定参数
		//分时电量 包含总电量 4*5=20
		//printf("m_iTOU start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_iTOU[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_iTOU[j]);
			//printf("meterData[%d].m_iTOU[%d]=%f\n",i,j,meterData[i].m_iTOU[j]);
		}
		//象限无功电能 4*5=20
		//printf("m_iQR start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){ //
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_iQR[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_iQR[j]);
			//printf("meterData[%d].m_iQR[%d]=%f\n",i,j,meterData[i].m_iQR[j]);
		}
		//最大需量 2*2*5=20
		//printf("m_iMaxN start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){ //
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_iMaxN[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_iMaxN[j]);
			//printf("meterData[%d].m_iMaxN[%d]=%f\n",i,j,meterData[i].m_iMaxN[j]);
		}
		//瞬时量 3+3
		//printf("Voltage start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PHASENUM;j++){//电压abc
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_wU[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_wU[j]);
			//printf("meterData[%d].m_wU[%d]=%f\n",i,j,meterData[i].m_wU[j]);
		}
		//printf("Current start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PHASENUM;j++){//电流abc
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_wI[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_wI[j]);
			//printf("meterData[%d].m_wI[%d]=%f\n",i,j,meterData[i].m_wI[j]);
		}
		//有功 /无功 /功率因数|总,a,b,c
		//printf("m_iP start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//有功
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_iP[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_iP[j]);
			//printf("meterData[%d].m_iP[%d]=%f\n",i,j,meterData[i].m_iP[j]);
		}
		//printf("m_wQ start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//无功
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_wQ[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_wQ[j]);
			//printf("meterData[%d].m_wQ[%d]=%f\n",i,j,meterData[i].m_wQ[j]);
		}
		//printf("m_wPF start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//功率因数
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_wPF[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_wPF[j]);
			//printf("meterData[%d].m_wPF[%d]=%f\n",i,j,meterData[i].m_wPF[j]);
		}
		for(j=0;j<PQCNUM;j++){//保留4个32位寄存器,可能需要
			dat2mbreg_lo16bit(&reg[base+sub++],(int)0xFFFFFFFF);
			dat2mbreg_hi16bit(&reg[base+sub++],(int)0xFFFFFFFF);
		}
		//printf("m_wPBCountstart at  line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		//断相记录 4(abc总)*2(次数,时间)
		for(j=0;j<PQCNUM;j++){//
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_wPBCount[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_wPBCount[j]);
			//printf("meterData[%d].m_wPBCount[%d]=%d\n",i,j,meterData[i].m_wPBCount[j]);

		}
		//断相总时间
		//printf("m_iPBTotalTime start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//
			dat2mbreg_lo16bit(&reg[base+sub++],meter[i].m_iPBTotalTime[j]);
			dat2mbreg_hi16bit(&reg[base+sub++],meter[i].m_iPBTotalTime[j]);
			//printf("meterData[%d].m_iPBTotalTime[%d]=%d\n",i,j,meterData[i].m_iPBTotalTime[j]);
		}

#if 0		//其他:
		dat2mbreg(&reg_tbl[base+sub++],meterData[i].m_cF);//short
		dat2mbreg(&reg_tbl[base+sub++],meterData[i].m_Ue);
		dat2mbreg(&reg_tbl[base+sub++],meterData[i].m_Ie);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].Flag_P3L4,meterData[i].FLAG_LP);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].FLAG_TIME,meterData[i].m_cMtrnunmb);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_cCommflag,meterData[i].m_comflag_first);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_cADDR[0],meterData[i].m_cADDR[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_cADDR[2],meterData[i].m_cADDR[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_cADDR[4],meterData[i].m_cADDR[5]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_com_Pwd[0],meterData[i].m_com_Pwd[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_com_Pwd[2],meterData[i].m_com_Pwd[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_cPortplan,meterData[i].m_cProt);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_time[0],meterData[i].m_time[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_time[2],meterData[i].m_time[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_time[4],meterData[i].m_time[5]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meterData[i].m_time[6],0);

#endif
	}//end for
	//printf("bytes of one loop ,endline: %d sub=0x%X\n",__LINE__,sub);
#endif

	return 0;
}
/*对应 0x10操作,把值从寄存器赋值给结构体变量
	预留的备用(扩展)功能
*/
int Cmbap::map_reg2dat(u16  reg_tbl[0xFFFF+1]
		       ,stMeter_Run_data meterData[]
		       ,const struct mb_write_req_pdu request_pdu) const
{
	int i;int addr;
	u16 start_addr=u16((request_pdu.start_addr_hi<<8)+request_pdu.start_addr_lo);
	u16 reg_quantity=u16((request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo);
	unsigned char meter_no=u8((start_addr & 0xFF00)>>8);
	unsigned char sub_id=u8((start_addr & 0xFF));
	u16 end_addr=u16(start_addr+reg_quantity-1);
	unsigned char meter_no_e=u8((end_addr & 0xFF00)>>8);
	meterData[0].Flag_Meter=0x87654321;
	printf("start_addr=0x%X meter_no=0x%X sub_id=0x%X end_addr=0x%X meter_no_e=%X"
	       ,start_addr,meter_no,sub_id,end_addr,meter_no_e);
	i=meter_no; //循环 表号
	addr=(i<<8);
	switch(sub_id){ //每个表的 子寄存器地址
begin:case 0x00:dat2mbreg_lo16bit(&reg_tbl[addr+0x00],meterData[i].Flag_Meter,1);
		if((addr + 0x00) >= end_addr) goto OK;
	case 0x01:dat2mbreg_hi16bit(&reg_tbl[addr+0x01],meterData[i].Flag_Meter,1);
		if((addr + 0x01) >= end_addr) goto OK;
	case 0x02:dat2mbreg_lo16bit(&reg_tbl[addr+0x02],meterData[i].Flag_TOU,1);
		if((addr + 0x02) >= end_addr) goto OK;
	case 0x03:dat2mbreg_hi16bit(&reg_tbl[addr+0x03],meterData[i].Flag_TOU,1);
		if((addr + 0x03) >= end_addr) goto OK;
	}
	i++;
	addr=(i<<8);
	goto begin;
OK:
	printf("meterData[1].Flag_Meter=0x%X meterData[0].Flag_TOU=%d \n"
	       ,meterData[1].Flag_Meter,meterData[0].Flag_TOU);
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
