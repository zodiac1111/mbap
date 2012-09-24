/*
	mdap.cpp:ModBus Application Protocol
	libmbap.so
	��ʵ����:0x06(���������16λ�Ĵ���)
	TODO:	 0x10(д���16λ�Ĵ���)
	������.
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


/***************************** �ӿ� ***********************/
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
	����/��֤:�� verify_* �������
	ִ��:	map_dat2reg
	���챨��: make_*_msg
	����:		response_*
*/
//TODO:�������߼�������ͬ�ĺ�������
int Cmbap::ReciProc(void)
{
	unsigned short  len=0;
	//TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes.
	u8 readbuf[260];
	bool verify_req=false;
	u8 errcode=0;
	//0. ����
	len=get_num(&m_recvBuf);
	if(len==0){
		return 0;
	}
	copyfrom_buf(readbuf, &m_recvBuf, len);
	//��֤1 ������֤
	if(verify_msg(readbuf,len)==false){
		printf(MB_PERFIX_ERR"reci illegal message\n");
		return -1;
	}
	syn_loopbuff_ptr(&m_recvBuf);//�������
	//���Ƴ�MBAPͷ
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//��֤2 mbapͷ��֤
	if(this->verify_mbap(req_mbap) == false){
		return -2;
	}
#ifdef DBG_SHOW_RECI_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);
	fflush(stdout);
#endif
	//��֤:������,
	u8 funcode=readbuf[sizeof(req_mbap)];
	if(verify_funcode(funcode)==false){
		printf("(%02X|NaN)\n",funcode);
		printf(MB_PERFIX_ERR ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",funcode);
		this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
				     funcode,ERR_ILLEGAL_FUN);
		this->send_excep_response();
	}
	//���ݹ����� �ж� ���Ƶ���ͬ�� ����pdu.
	switch (funcode){
	/******************************************************/
	case MB_FUN_R_HOLD_REG ://��������ּĴ���
		memcpy(&read_req_pdu,&readbuf[0]+sizeof(req_mbap)
		       ,sizeof(read_req_pdu));
#ifdef DBG_SHOW_RECI_MSG
		print_req_pdu(read_req_pdu);
		printf("\n");
#endif
		//��֤3 pdu��֤:������,�Ĵ�����ַ/����(���쳣,�����쳣pdu)
		verify_req= verify_req_pdu(read_req_pdu,errcode);
		if(verify_req==false){
			//��֤����
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,errcode);
			//����
			this->send_excep_response();
			return -3;
		}
		//�������� m_meterData �и������ݵ� reg-table
		if(this->map_dat2reg(this->reg_table,this-> m_meterData,read_req_pdu) != 0 ){
			// ִ���쳣
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,
					     ERR_SLAVE_DEVICE_FAILURE);
			//����
			this->send_excep_response();
			return -4;
		}
		//���챨��
		if(make_msg(this->req_mbap,this->read_req_pdu
			    ,this->rsp_mbap,this->read_rsp_pdu
			    ,this->ppdu_dat) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return -4;
		}
		//����
		this->send_response(rsp_mbap,read_rsp_pdu,
				    &ppdu_dat[0],m_transBuf);
		break;
	/******************************************************/
	case MB_FUN_W_MULTI_REG://д����Ĵ��� ����:�ο��ĵ�[3].p31
		memcpy(&write_req_pdu,&readbuf[0]+sizeof(req_mbap)//pdu
		       ,sizeof(write_req_pdu));
#ifdef DBG_SHOW_RECI_MSG
		print_req_pdu(write_req_pdu);
		fflush(stdout);
#endif
		verify_req= verify_req_pdu(write_req_pdu,errcode);
		if(verify_req==false){
			//��֤����
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     write_req_pdu.func_code,errcode);
			//����
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
		//д�Ĵ�������
		reg_table[0xfffb]=0x1234;
		reg_table[0xfffc]=0x5678;
		reg_table[0xfffd]=0x9abc;
		reg_table[0xfffe]=0xdef0;
		reg_table[0xffff]=u16((ppdu_dat[0]<<8)+ppdu_dat[1]);
		//���챨��
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
//�����쳣���ر���
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

//�����쳣���ر���
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

/*	���ͱ���(������ 0x06) ���нṹ���Ƕ��� 0x06���ܵ�
	����: response_mbap  response_pdu  pdu_dat[255]
	���: transBuf(struct)	���͵ı���.
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
	//��Ӧ���� send msg (to m_transBuf.m_transceiveBuf ����)
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
/*	���ͱ���(������ 0x06) ���нṹ���Ƕ��� 0x06���ܵ�
	����: response_mbap  response_pdu
	���: transBuf(struct)	���͵ı���.
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
	//��Ӧ���� send msg (to m_transBuf.m_transceiveBuf ����)
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

/*	������Ӧ 0x06 �ķ��صı���
	����:	request_mbap (const)	����� mbap
		request_pdu  (const)	����� pdu
	���:	respond_mbap(&mbap_head)��Ӧ�� mbap
		respond_pdu(&mbap_head)	��Ӧ�� (����)pdu
		pdu_dat (u8[255])	���ر������� pdu-dat
	return:	0-�ɹ� other-ʧ��
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_read_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_read_rsp_pdu &respond_pdu
		     ,u8 pdu_dat[255])const
{
	int i; //����ǰ��׼������
	int start_addr=request_pdu.start_addr_hi*256+request_pdu.start_addr_lo;
	int reg_quantity=request_pdu.reg_quantity_hi*256+request_pdu.reg_quantity_lo;
	respond_pdu.byte_count=u8(reg_quantity*BYTE_PER_REG);
	//����mbap pdu ��pdu-dat ��׼������
	//mbap
	respond_mbap.TransID_hi=request_mbap.TransID_hi;//���и�
	respond_mbap.TransID_lo=request_mbap.TransID_lo;//���е�
	respond_mbap.protocolhead_hi=request_mbap.protocolhead_hi;//Э���
	respond_mbap.protocolhead_lo=request_mbap.protocolhead_lo;//Э���
	respond_mbap.len_hi=request_mbap.len_hi;//���ȸ�(Ӧ��Ϊ0)
	respond_mbap.len_lo=u8(sizeof(respond_mbap.unitindet) //���ȵ�= 1byt�Ĵ�վ��ַ
			       +sizeof(respond_pdu) // + 2byte�� funcode+datlen
			       +respond_pdu.byte_count); // + dat len(byte)
	respond_mbap.unitindet=request_mbap.unitindet;
	//pdu
	respond_pdu.func_code=request_pdu.func_code;
	respond_pdu.byte_count=u8(reg_quantity*BYTE_PER_REG);
	//pdu-dat
	for(i=0;i<reg_quantity;i++){
		//modbus16λ�Ĵ�������˳��:���ֽ���ǰ,���ֽ��ں�
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
/*	������Ӧ 0x10 �ķ��صı���
	����:	request_mbap (const)	����� mbap
		request_pdu  (const)	����� pdu
	���:	respond_mbap(&mbap_head)��Ӧ�� mbap
		respond_pdu(&mbap_head)	��Ӧ�� (����)pdu
	return:	0-�ɹ� other-ʧ��
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_write_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_write_rsp_pdu &respond_pdu)const
{
	//����mbap  ��pdu ��׼������
	//mbap
	respond_mbap.TransID_hi=request_mbap.TransID_hi;//���и�
	respond_mbap.TransID_lo=request_mbap.TransID_lo;//���е�
	respond_mbap.protocolhead_hi=request_mbap.protocolhead_hi;//Э���
	respond_mbap.protocolhead_lo=request_mbap.protocolhead_lo;//Э���
	respond_mbap.len_hi=request_mbap.len_hi;//���ȸ�(Ӧ��Ϊ0)
	respond_mbap.len_lo=u8(sizeof(respond_mbap.unitindet) //���ȵ�= 1byt�Ĵ�վ��ַ
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

//	������֤ 1	:��֤��Ϣ��(����),���峤��(����7+1������)
bool Cmbap::verify_msg(u8* recvBuf, unsigned short len) const
{
	if(len<(sizeof(struct mbap_head)+1 /*funcode*/)){
		printf(MB_PERFIX_ERR"mbap msg len:%d is less than 7.\n",len);
		return false;
	}
	return true;
}

/*	������֤ 2
	��֤mbapͷ�ĺϷ��� �Ϸ����� true ���򷵻� false
	ref: http://modbus.control.com/thread/1026162917
*/
bool Cmbap::verify_mbap(const struct mbap_head request_mbap) const
{
	//1. ���к�:
	//	��վ:����ǰ2���ֽ�,�������� ��������
	//	������վ,����Ҫ������к��Ƿ�ʱ֮ǰ�Լ����͵ĵ����к�.
	//2. Э��: 0x0000 Ϊ modbus
	if(!(request_mbap.protocolhead_hi==0x00
	     && request_mbap.protocolhead_lo==0x00)){
		printf(MB_PERFIX_ERR"mbap protocol not modbus/tcp protocol.\n");
		return false;
	}
	//3. ����:
	// verify 5th byte is zero (upper length byte = zero as length MUST be
	// less than 256) ��֤����
	if(request_mbap.len_hi != 0x00 ){
		printf(MB_PERFIX_ERR"mbap the length of msg is too long.\n");
		return false;
	}
	//4. ��վ��� [2,253]
	//verify 6th byte is between 2 to 253. min cmd length is unit id
	if(!(request_mbap.unitindet>=2 && request_mbap.unitindet<= 253)){
		printf(MB_PERFIX_ERR"slave address (%d) is out of range.\n"
		       ,request_mbap.unitindet);
		return false;
	}
	return true;
}

/*	������֤ 3 pdu��Ԫ,�Ĵ�����ַ/������֤
	��֤ req_pdu
	����:	req_pdu
	���:	excep_rsp_pdu(���Ϸ�ʱ���� | �Ϸ�ʱ����)
	����ֵ	: true:��֤ͨ��	fasle:���Ϸ�
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


/*	������֤ 3-1
	��֤������
	�жϹ������Ƿ�Ƿ�,δʵ�ֵĹ���Ҳ�ǲ��Ϸ�
	param : ������
	return: true:ͨ����֤	false:���Ϸ�
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
	default://���������벻��֧��
		break;
	}
	return false;
}
/*	������֤ 3-2 ��֤ �Ĵ������� �Ƿ������Ӧ�Ĺ�����
	����:	req_pdu
	���:	reg_quantity�Ĵ�������
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
/*	������֤ 3-3 ��֤ �Ĵ��� ��ַ �Ƿ������Ӧ�Ĺ�����
	����:	req_pdu
	���:	start_addr ��ʼ��ַ end_addr ������ַ
*/
bool Cmbap::verify_reg_addr(const struct mb_read_req_pdu request_pdu,
			    int &start_addr,int &end_addr )const
{
	int reg_quantity=0;
	reg_quantity=request_pdu.reg_quantity_hi*255+request_pdu.reg_quantity_lo;
	start_addr=request_pdu.start_addr_hi*255+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity;
	// ��ʼ�ͽ�����ַ[0x0000,0xFFFF]
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
	// ��ʼ�ͽ�����ַ[0x0000,0xFFFF]
	if(start_addr<0x0000 || start_addr >0xFFFF
			|| end_addr<0x0000 || end_addr >0xFFFF ){
		return false;
	}

	return true;
}
/********************* һϵ�����ݸ�ʽת������ **************************/
//��32λ int ������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ �ĸ�16λ
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const unsigned int dat32) const
{
	reg[0]=u16((dat32 & 0xFFFF0000)>>16);//��16bit
}
//��32λ int ������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const unsigned int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //��16bit
}
//��32λ in t������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const signed int dat32) const
{
	reg[0]=u16((dat32 & 0xFFFF0000)>>16);//��2�ֽ��ں�
}
//��32λ in t������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const signed int dat32) const
{
	reg[0]=u16((dat32 & 0x0000FFFF)>>0); //��2�ֽ���ǰ(���˳����modbus������)
}
//��32λ float ������ ת�������� 2�� modbus �Ĵ���(16λ)��
//IEEE 754 float ��ʽ,����ת�� http://babbage.cs.qc.cuny.edu/IEEE-754/
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const float float32) const
{
	//IEEE 754 float ��ʽ,����ת�� http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=u16(( *(int *)&float32 & 0xFFFF0000)>>16);
}
//��32λ float ������ ת�������� 2�� modbus �Ĵ���(16λ)��
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1],const float float32) const
{
	reg[0]=u16(( *(int *)&float32 & 0x0000FFFF)>>0);
}
//��16λ short ������ ת����Ϊ 1�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[1],const short dat16) const
{
	reg[0]=dat16;
}
//�� 2 �� 8λ char ���� �ϳɳ�Ϊ 1�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg(u16 reg[1]
			     ,const char high_byte,const char low_byte) const
{
	reg[0]=u16( (high_byte<<8) + low_byte) ;
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
inline void Cmbap::print_req_pdu(const struct mb_read_req_pdu request_pdu)const
{
	printf("(%02X|%02X %02X|%02X %02X)"
	       ,request_pdu.func_code
	       ,request_pdu.start_addr_hi
	       ,request_pdu.start_addr_lo
	       ,request_pdu.reg_quantity_hi
	       ,request_pdu.reg_quantity_lo);
}
//��ӡ����pdu
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
//��ӡ�쳣��Ӧpdu
inline void Cmbap::print_rsp_pdu(
		const struct mb_excep_rsp_pdu excep_respond_pdu)const
{
	printf("(ERR:%02X|%02X)"
	       ,excep_respond_pdu.exception_func_code
	       ,excep_respond_pdu.exception_code);
}
//��ӡ������Ӧpdu 0x06 x010
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
//��ӡ��Ӧpdu������
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
	for(i=meter_no;i<meter_no_e;i++){ //ѭ�� ���
		addr=(i<<8);
		switch(sub_id){ //ÿ����� �ӼĴ�����ַ
		case 0x00:dat2mbreg_lo16bit(&reg_tbl[addr+0x00],meterData[i].Flag_Meter);
			if((i<<8 & 0x00) ==end_addr) goto OK;
		case 0x01:dat2mbreg_hi16bit(&reg_tbl[addr+0x01],meterData[i].Flag_Meter);
			if((i<<8 & 0x01) ==end_addr) goto OK;
		}
	}
OK:
	return 0;
}
/*	��һЩ��Ҫ�����ݴ� stMeter_Run_data �ṹ���и���(ӳ��)��
	reg_table �Ĵ������Ա��ȡ��Щ����
	TODO: Ŀǰ��վ����һ��,�⸴��ȫ���������Ĵ�������,Ӧ�Ľ�Ч��!
	����:	struct stMeter_Run_data m_meterData[MAXMETER]
	���:	u16  reg_table[0xFFFF]
*/
int Cmbap::map_dat2reg(u16  reg_tbl[0xFFFF]
		       ,stMeter_Run_data meterData[]
		       ,const struct mb_read_req_pdu request_pdu) const

//���� DBG_REG_DAT ��Ҫ�޸Ľṹ��Ա������,���Բ�ʹ�� const ����
{
#ifdef DBG_REG_DAT //���Ը�����������
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
	//�����ǵ�Ч�汾:��Ч�汾TODO
	for (i=0;i<MAXMETER;i++) {
	//for (i=0;i<1;i++){
		addr=(i<<8);//���ֽڱ�ʾ���,�ֱ������ͬ�ı�,��Χ[0,MAXMETER]
		//���ֽڱ�ʾ��������,modbus�Ĵ���16λ,����int��ռ�������Ĵ���
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
/* �ο��ĵ�:
	1. refer: modbus/TCP http://www.simplymodbus.ca/TCP.htm
	2. http://www.electroind.com/pdf/Modbus_messaging_on_TCPIP_implementation_guide_V11.pdf
	3. http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf (����)
	4. http://www.modbus.org/docs/Modbus_Messaging_Implementation_Guide_V1_0b.pdf
	5. http://www.modbus.org/specs.php (˵����)
	6. ��Modbus��͸������ ��? ���� ��8��
*/
