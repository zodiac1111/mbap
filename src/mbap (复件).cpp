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
//����ѡ��:
//#define REG_DAT_DEBUG //ǿ�����ýṹ������,�鿴�Ĵ�������(�����紫��)��ȷ��
#define SHOW_INPUT_MSG //���ն���ʾ���յ� ��Ϣ(����)
#define SHOW_SEND_MSG //���ն���ʾ ���͵� ��Ϣ(����)
/***************************** �ӿ� ***********************/
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

void Cmbap::m_BroadcastTime(void)// �㲥
{
	return;
}

/*	TODO: ��վ��������:
	��ǰû�б�����,���ܵĻ�����������վ�����ϴ�ĳЩ������Ϣ,
	modbus���ڴ�ͳ��ʽ�ǲ�֧�ִ�վ�����ϴ���,�����modbus/tcp�ж���.
*/
void Cmbap::SendProc(void)
{
	return;
}

/*	��վ������վ�Ľ��պ���:
	��ģʽ�Ǵ�ͳmodbus/���ں�modbus/tcp���е�.
	����->����->ִ��->���� ���ɱ�����ʵ��
*/
int Cmbap::ReciProc(void)
{
	u8 len=0;
	//TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
	u8 readbuf[260];
	bool normal_response=false;//ָʾ��Ӧ Ϊ ������Ӧ
	//0. ����
	len=get_num(&m_recvBuf);
	if(len==0){
		return 0;
	}
	copyfrom_buf(readbuf, &m_recvBuf, len);
	//��֤1 ������֤
	if(verify_msg(readbuf,len)==false){
		printf(MB_ERR_PERFIX"reci illegal message\n");
		return -1;
	}
	syn_loopbuff_ptr(&m_recvBuf);//�������
	//1.	���Ƴ�MBAPͷ
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//��֤2 mbapͷ��֤
	if(this->verify_mbap(req_mbap) == false){
		return -2;
	}
	//2.	���Ƴ����� pdu
	memcpy(&req_pdu,&readbuf[0]+sizeof(req_mbap),sizeof(req_pdu));
#ifdef SHOW_INPUT_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);
	print_req_pdu(req_pdu);printf("\n");
#endif
	//��֤3 pdu��֤:������,�Ĵ�����ַ/����
	bool verify_req= verify_req_pdu(req_pdu,excep_rsp_pdu);
	if(verify_req==false){
		//�쳣����(�����쳣����)
		normal_response=false;
		memcpy(&rsp_mbap,&req_mbap,sizeof(req_mbap));
		rsp_mbap.len_lo=sizeof(rsp_mbap.unitindet)+sizeof(excep_rsp_pdu);
		//����
		this->send_response(normal_response);
		return -3;
	}
	//��������
	normal_response=true;
	//��������
	map_dat2reg(this->reg_table,this-> m_meterData);//�� m_meterData �и������ݵ� reg-table
	//���챨��
	if(make_msg(this->req_mbap,this->req_pdu,this->ppdu_dat) != 0){
		printf(MB_ERR_PERFIX"make msg\n");
		return -4;
	}
	//����
	this->send_response(normal_response);
	return 0;
}

/*	���ͱ���
	����: normal_response(bool)�Ƿ�Ϊ �������ر���
		��Ա���� mbap,��Ӧpdu,��Ӧpdu-dat
	���: m_transBuf(struct)	���͵ı���.
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
	//��Ӧ���� send msg (to m_transBuf.m_transceiveBuf ����)
	if(normal_response){//������Ӧ����
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
	}else{//�쳣���ذ�
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

/*	�������صı��� TODO: ����������� �������
	����:	req_mbap (const)	����� mbap
		req_pdu  (const)	����� pdu
	���:	ppdu_dat (*&u8)		���ر������� pdu-dat
	return:	0-�ɹ� other-ʧ��
  */
int Cmbap::make_msg( const struct mbap_head req_mbap
		     ,const struct mb_req_pdu req_pdu
		     ,u8 ppdu_dat[])
{
	int i; //����ǰ��׼������
	int start_addr=req_pdu.start_addr_hi*256+req_pdu.start_addr_lo;
	int reg_quantity=req_pdu.reg_quantity_hi*256+req_pdu.reg_quantity_lo;
	rsp_pdu.byte_count=reg_quantity*BYTE_PER_REG;
	//����mbap pdu ��pdu-dat ��׼������
	//mbap
	rsp_mbap.TransID_hi=req_mbap.TransID_hi;//���и�
	rsp_mbap.TransID_lo=req_mbap.TransID_lo;//���е�
	rsp_mbap.protocolhead_hi=req_mbap.protocolhead_hi;//Э���
	rsp_mbap.protocolhead_lo=req_mbap.protocolhead_lo;//Э���
	rsp_mbap.len_hi=req_mbap.len_hi;//���ȸ�(Ӧ��Ϊ0)
	rsp_mbap.len_lo=sizeof(rsp_mbap.unitindet) //���ȵ�= 1byt�Ĵ�վ��ַ
			+sizeof(rsp_pdu) // + 2byte�� funcode+datlen
			+rsp_pdu.byte_count; // + dat len(byte)
	rsp_mbap.unitindet=req_mbap.unitindet;
	//pdu
	rsp_pdu.func_code=req_pdu.func_code;
	rsp_pdu.byte_count=reg_quantity*BYTE_PER_REG;
	//pdu-dat
	for(i=0;i<reg_quantity;i++){
		//modbus16λ�Ĵ�������˳��:���ֽ���ǰ,���ֽ��ں�
		ppdu_dat[i*2+0]=(reg_table[start_addr+i] & 0xFF00)>>8;
		ppdu_dat[i*2+1]=(reg_table[start_addr+i] & 0x00FF);
	}
	return 0;
}

//	������֤ 1	:��֤��Ϣ��(����),���峤��
bool Cmbap::verify_msg(u8* m_recvBuf,const u8 len) const
{
	if(len<7){
		printf(MB_ERR_PERFIX"mbap msg len:%d is less than 7.\n",len);
		return false;
	}
	//���ĺ���ĳ���
	int req_msg_len=sizeof(struct mbap_head)+sizeof(struct mb_req_pdu);
	if(len!=req_msg_len){//���ĳ��ȼ��
		printf(MB_ERR_PERFIX"reci len=%d,it should be %d.\n",len, req_msg_len);
		return false;
	}
	return true;
}

/*	������֤ 2
	��֤mbapͷ�ĺϷ��� �Ϸ����� true ���򷵻� false
	ref: http://modbus.control.com/thread/1026162917
*/
bool Cmbap::verify_mbap(const struct mbap_head req_mbap) const
{
	//1. ���к�:
	//	��վ:����ǰ2���ֽ�,�������� ��������
	//	������վ,����Ҫ������к��Ƿ�ʱ֮ǰ�Լ����͵ĵ����к�.
	//2. Э��: 0x0000 Ϊ modbus
	if(!(req_mbap.protocolhead_hi==0x00
	     && req_mbap.protocolhead_lo==0x00)){
		printf(MB_ERR_PERFIX"mbap protocol not modbus/tcp protocol.\n");
		return false;
	}
	//3. ����:
	// verify 5th byte is zero (upper length byte = zero as length MUST be
	// less than 256) ��֤����
	if(req_mbap.len_hi != 0x00 ){
		printf(MB_ERR_PERFIX"mbap the length of msg is too long.\n");
		return false;
	}
	//4. ��վ��� [2,253]
	//verify 6th byte is between 2 to 253. min cmd length is unit id
	if(!(req_mbap.unitindet>=2 && req_mbap.unitindet<= 253)){
		printf(MB_ERR_PERFIX"slave address (%d) is out of range.\n"
		       ,req_mbap.unitindet);
		return false;
	}
	return true;
}
/*	������֤ 3 pdu��Ԫ,������,�Ĵ�����ַ/������֤
	��֤ req_pdu
	����:	req_pdu
	���:	excep_rsp_pdu(���Ϸ�ʱ���� | �Ϸ�ʱ����)
	����ֵ	: true:��֤ͨ��	fasle:���Ϸ�
*/
bool Cmbap:: verify_req_pdu(const struct mb_req_pdu req_pdu,
			    struct mb_excep_rsp_pdu &excep_rsp_pdu)
{
	u8 errcode=0;int reg_quantity; int start_addr;int end_addr;
	//1. �������Ƿ�֧�� is_funcode_supported(req_pdu.func_code)
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
	//�쳣������=������+0x80 �ο���׼
	excep_rsp_pdu.exception_func_code=req_pdu.func_code+0x80;
	return false;
}

/*	������֤ 3-1
	��֤������
	�жϹ������Ƿ�Ƿ�,δʵ�ֵĹ���Ҳ�ǲ��Ϸ�
	param : ������
	return: true:ͨ����֤	false:���Ϸ�
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
	default://���������벻��֧��
		return false;
		break;
	}
	return false;
}
/*	������֤ 3-2 ��֤ �Ĵ������� �Ƿ������Ӧ�Ĺ�����
	����: req_pdu	���:reg_quantity�Ĵ�������
*/
inline bool Cmbap::verify_reg_quantity(const struct mb_req_pdu req_pdu
				       ,int &reg_quantity)const
{
	bool ret=true;
	reg_quantity=req_pdu.reg_quantity_hi*255+req_pdu.reg_quantity_lo;
	switch(req_pdu.func_code){
	case MB_FUN_R_HOLD_REG:	//�Ĵ�����Ŀ[0x0001,0x07D]
		if(reg_quantity<0x0001 || reg_quantity>0x007D)
			ret=false;
		break;
	default:
		ret=false;
	}
	return ret;
}
/*	������֤ 3-3 ��֤ �Ĵ��� ��ַ �Ƿ������Ӧ�Ĺ�����
	����: req_pdu ���: start_addr ��ʼ��ַ end_addr ������ַ
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
		// ��ʼ�ͽ�����ַ[0x0000,0xFFFF]
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

/********************* һϵ�����ݸ�ʽת������ **************************/
//��32λ in t������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[2],const unsigned int dat32)
{
	reg[0]=(dat32 & 0x0000FFFF)>>0; //��2�ֽ���ǰ(���˳����modbus������)
	reg[1]=(dat32 & 0xFFFF0000)>>16;//��2�ֽ��ں�
}
//��32λ in t������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[2],const signed int dat32)
{
	reg[0]=(dat32 & 0x0000FFFF)>>0; //��2�ֽ���ǰ(���˳����modbus������)
	reg[1]=(dat32 & 0xFFFF0000)>>16;//��2�ֽ��ں�
}
//��32λ float ������ ת�������� 2�� modbus �Ĵ���(16λ)��
inline void Cmbap::dat2mbreg(u16 reg[2],const float float32)
{
	//IEEE 754 float ��ʽ,����ת�� http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=( *(int *)&float32 & 0x0000FFFF)>>0;
	reg[1]=( *(int *)&float32 & 0xFFFF0000)>>16;
}
//��16λ short ������ ת����Ϊ 1�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[1],const short dat16)
{
	reg[0]=dat16;
}
//�� 2 �� 8λ char ���� �ϳɳ�Ϊ 1�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[1]
			     ,const char high_byte,const char low_byte)
{
	reg[0]= (high_byte<<8) + low_byte ;
}

/************************ һϵ�д�ӡ���� *******************************/
//��ӡ mbap ͷ��Ϣ �������Ӧ ��ʽ��ͬ
void Cmbap::print_mbap( const struct mbap_head mbap) const
{
	printf("{%02X %02X|%02X %02X|%02X %02X|%02X}"
	       ,mbap.TransID_hi,mbap.TransID_lo //������
	       ,mbap.protocolhead_hi,mbap.protocolhead_lo //Э��
	       ,mbap.len_hi,mbap.len_lo //����
	       ,mbap.unitindet); //��Ԫ
}
//��ӡ����pdu
inline void Cmbap::print_req_pdu(const struct mb_req_pdu req_pdu)const
{
	printf("(%02X|%02X %02X|%02X %02X)"
	       ,req_pdu.func_code
	       ,req_pdu.start_addr_hi
	       ,req_pdu.start_addr_lo
	       ,req_pdu.reg_quantity_hi
	       ,req_pdu.reg_quantity_lo);
}
//��ӡ�쳣��Ӧpdu
inline void Cmbap::print_excep_rsp_pdu(struct mb_excep_rsp_pdu excep_rsp_pdu)const
{
	printf("(ERR:%02X|%02X)"
	       ,excep_rsp_pdu.exception_func_code
	       ,excep_rsp_pdu.exception_code);
}
//��ӡ��Ӧpdu
inline void Cmbap::print_rsp_pdu(const struct mb_rsp_pdu rsp_pdu) const
{
	printf("(%02X|%02X)"
	       ,rsp_pdu.func_code
	       ,rsp_pdu.byte_count);
}
//��ӡ��Ӧpdu������
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

/*	��һЩ��Ҫ�����ݴ� stMeter_Run_data �ṹ���и���(ӳ��)��
	reg_table �Ĵ������Ա��ȡ��Щ����
	����:	struct stMeter_Run_data m_meterData[MAXMETER]
	���:	u16  reg_table[0xFFFF]
*/
int Cmbap::map_dat2reg(u16  reg_table[0xFFFF]
		       ,/*const*/ stMeter_Run_data m_meterData[])
//���� REG_DAT_DEBUG ��Ҫ�޸Ľṹ��Ա������,���Բ�ʹ�� const ����
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
		addr=(i<<8);//���ֽڱ�ʾ���,�ֱ������ͬ�ı�,��Χ[0,MAXMETER]
		//���ֽڱ�ʾ��������,modbus�Ĵ���16λ,����int��ռ�������Ĵ���
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
