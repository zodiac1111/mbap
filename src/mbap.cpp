/*
	mdap.cpp:ModBus Application Protocol
	libmbap.so
	��ǰ��ʵ����:0x06(���������16λ�Ĵ���)
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
#include "metershm.h"
/***************************** �ӿ� ***********************/
extern "C" CProtocol *CreateCProto_Cmbap(void)
{
	//static_assert(sizeof(int) == 4, "Integer sizes expected to be 4");
	return new Cmbap;
}

Cmbap::Cmbap()
{

}

Cmbap::~Cmbap()
{
	//printf(MB_PERFIX"disconstruct class\n");
}
//�ᱻ����
int Cmbap::Init(struct stPortConfig *tmp_portcfg){
	if((tmp_portcfg->m_ertuaddr>>8) != 0x00){
		printf(MB_PERFIX_ERR
		       "Slave ID len is more than 1 Byte"
		       ",please set less than 0xFF(255)\n");
		return -2;
	}
	this->slave_ID=(unsigned char)tmp_portcfg->m_ertuaddr;//�ն˱�ŵ��ֽ�
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

void Cmbap::m_BroadcastTime(void)// �㲥(һ��Э���վ��ַ0x255Ϊ�㲥)
{
	return;
}

/*	TODO: ��վ��������(����):
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
	ִ��:	map_dat2reg(0x06) / map_reg2dat(0x10,δ���)
	���챨��: make_msg[_excep]
	����:		response_*
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
	//0. ����
	len=get_num(&m_recvBuf);
	if(len==0){
		return 0;
	}
	copyfrom_buf(readbuf, &m_recvBuf, len);
	//��֤1 ������֤
	if( verify_msg(len)==false ){
		printf(MB_PERFIX_ERR"reci illegal message\n");
		return 0;
	}
	syn_loopbuff_ptr(&m_recvBuf);
	//�������,���б��Ĵ���:����,(ִ��)����
	//���Ƴ�MBAPͷ
	memcpy(&req_mbap,&readbuf[0],sizeof(req_mbap));
	//���slave_ID�ж��Ƿ��Ƿ��͸�����վ��
	if(req_mbap.unitindet != slave_ID ){
		printf("slave_ID=%d req_mbap.unitindet =%d",slave_ID,req_mbap.unitindet );
		return 0;
	}
	//��֤ mbapͷ
	if(this->verify_mbap(req_mbap) == false){
		printf(MB_PERFIX_ERR"verify_mbap\n");
		return 0;
	}
#ifdef DBG_SHOW_RECI_MSG
	printf(MB_PERFIX"<<< Reci form master:");
	print_mbap(req_mbap);fflush(stdout);//stdoutΪ�л����豸,ǿ��ˢ��
#endif
	//��֤ ������,
	u8 funcode=readbuf[sizeof(req_mbap)];
	if(verify_funcode(funcode)==false){
		printf("(%02X|NaN)\n",funcode);
		printf(MB_PERFIX_ERR ERR_ILLEGAL_FUN_MSG);
		printf(" function code: 0x%02X.\n",funcode);
		make_msg_excep(rsp_mbap,excep_rsp_pdu,funcode,ERR_ILLEGAL_FUN);
		send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
	}
	//���ݹ����� �ж� ���Ƶ���ͬ�� ����pdu.
	switch (funcode){
	/********************** 0x06 ******************************/
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
			this->send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
			return 0;
		}

		//�������� m_meterData �и������ݵ� reg-table
		if(this->map_dat2reg(this->reg_table,m_meterData,read_req_pdu) != 0 ){

			// ִ���쳣
			this->make_msg_excep(rsp_mbap,excep_rsp_pdu,
					     read_req_pdu.func_code,
					     ERR_SLAVE_DEVICE_FAILURE);
			//����
			this->send_response_excep(req_mbap,excep_rsp_pdu,m_transBuf);
			return 0;
		}
		//���챨��
		if(make_msg(this->req_mbap,this->read_req_pdu
			    ,this->rsp_mbap,this->read_rsp_pdu
			    ,this->ppdu_dat) != 0){
			printf(MB_PERFIX_ERR"make msg\n");
			return 0;
		}
		//����
		this->send_response(rsp_mbap,read_rsp_pdu,
				    &ppdu_dat[0],m_transBuf);
		break;
		/********************** 0x10 *****************************/
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
		//д�Ĵ������� (DEMO)
		start_addr=(write_req_pdu.start_addr_hi<<8)
				+write_req_pdu.start_addr_lo;
		reg_quantity=(write_req_pdu.reg_quantity_hi<<8)
				+write_req_pdu.reg_quantity_lo;
		for(i=0;i<reg_quantity;i++){
			reg_table[start_addr+i]=u16((ppdu_dat[i*2+0]<<8)+ppdu_dat[i*2+1]);
			printf("regtable=%X ;",reg_table[start_addr+i]);
		}
		map_reg2dat(reg_table,m_meterData,write_req_pdu);
		//���챨��
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

/*	���ͱ���(������ 0x06) ���нṹ���Ƕ��� 0x06���ܵ�
	����: response_mbap  response_pdu  pdu_dat[256]
	���: transBuf(struct)	���͵ı���.
*/
int Cmbap::send_response(const struct mbap_head mbap
			 ,const struct mb_read_rsp_pdu pdu
			 ,const rsp_dat pdu_dat[256]
			 ,TransReceiveBuf &transBuf)const
{
	//��Ӧ���� send msg (to m_transBuf.m_transceiveBuf ����)
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
/*	���ͱ���(������ 0x10) ���нṹ���Ƕ��� 0x10���ܵ�
	����: response_mbap  response_pdu
	���: transBuf(struct)	���͵ı���.
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
//�����쳣���ر���
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
/*	������Ӧ 0x06 �ķ��صı���
	����:	request_mbap (const)	����� mbap
		request_pdu  (const)	����� pdu
	���:	respond_mbap(&mbap_head)��Ӧ�� mbap
		respond_pdu(&mbap_head)	��Ӧ�� (����)pdu
		pdu_dat (u8[256])	���ر������� pdu-dat
	return:	0-�ɹ� other-ʧ��
  */
int Cmbap::make_msg( const struct mbap_head request_mbap
		     ,const struct mb_read_req_pdu request_pdu
		     ,struct mbap_head &respond_mbap
		     ,struct mb_read_rsp_pdu &respond_pdu
		     ,u8 pdu_dat[256])const
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
		printf("%04X = %02X ,%02X\n"
		       ,reg_table[start_addr+i],pdu_dat[i*2+0],pdu_dat[i*2+1]);
#endif
	}
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
//�����쳣���ر���
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
//	������֤ :��֤��Ϣ��(����),���峤��(����7+1������)
bool Cmbap::verify_msg( unsigned short len) const
{
	if(len<(sizeof(struct mbap_head)+1 /*funcode*/)){
		printf(MB_PERFIX_ERR"mbap msg len:%d is less than 7.\n",len);
		return false;
	}
	return true;
}

/*	������֤ :��֤mbapͷ�ĺϷ��� �Ϸ����� true ���򷵻� false
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
/*	������֤
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
	case MB_FUN_W_MULTI_REG:
		return false;
		break;
		/* .add other funcode here */
	default://���������벻��֧��
		break;
	}
	return false;
}

/*	������֤  pdu��Ԫ,�Ĵ�����ַ/������֤
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

/*	������֤:��֤ �Ĵ������� �Ƿ������Ӧ�Ĺ�����
	����:	req_pdu
	���:	reg_quantity�Ĵ�������
*/
bool Cmbap::verify_reg_quantity(const struct mb_read_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=(request_pdu.reg_quantity_hi<<8)
			+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007D){
		return false;
	}
	return true;
}
bool Cmbap::verify_reg_quantity(const struct mb_write_req_pdu request_pdu
				,int &reg_quantity)const
{
	reg_quantity=(request_pdu.reg_quantity_hi<<8)
			+request_pdu.reg_quantity_lo;
	if(reg_quantity<0x0001 || reg_quantity>0x007B){
		return false;
	}
	if(request_pdu.byte_count != (reg_quantity<<1) ){
		printf("cnt=%d quan= %d",request_pdu.byte_count,reg_quantity);
		return false;
	}
	return true;
}
/*	������֤ :��֤ �Ĵ��� ��ַ �Ƿ������Ӧ�Ĺ�����
	����:	req_pdu
	���:	start_addr ��ʼ��ַ end_addr ������ַ
*/
bool Cmbap::verify_reg_addr(const struct mb_read_req_pdu request_pdu,
			    int &start_addr,int &end_addr )const
{
	int reg_quantity=0;
	reg_quantity=(request_pdu.reg_quantity_hi << 8)
			+request_pdu.reg_quantity_lo;
	start_addr=(request_pdu.start_addr_hi << 8) +request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity-1;
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
	reg_quantity=(request_pdu.reg_quantity_hi<<8)
			+request_pdu.reg_quantity_lo;
	start_addr=(request_pdu.start_addr_hi<<8)+request_pdu.start_addr_lo;
	end_addr=start_addr+reg_quantity-1;
	// ��ʼ�ͽ�����ַ[0x0000,0xFFFF]
	if(start_addr<0x0000 || start_addr >0xFFFF
			|| end_addr<0x0000 || end_addr >0xFFFF ){
		return false;
	}
	return true;
}
/********************* һϵ�����ݸ�ʽת������ **************************/
//��32λ int ������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ �ĸ�16λ
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1], unsigned int &dat32,int dir=0)
const
{
	if(dir==0){
		reg[0]=u16((dat32 & 0xFFFF0000)>>16);//��16bit
	}else{
		dat32=(dat32 & 0x0000FFFF) | (reg[0]<<16);
		printf("dat32=%X reg=%X ",dat32,reg[0]);
	}
}
//��32λ int ������ ת����Ϊ 2�� modbus�Ĵ���(16λ)����ʽ
inline void Cmbap::dat2mbreg_lo16bit(u16 reg[1], unsigned int &dat32,int dir=0)
const
{
	if(dir==0){
		reg[0]=u16((dat32 & 0x0000FFFF)>>0); //��16bit
	}else{
		dat32=(dat32 & 0xFFFF0000) | reg[0];
		printf("dat32=%X reg=%X ",dat32,reg[0]);
	}
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
//��32λ float ������ ת�������� 2�� modbus �Ĵ���(16λ)��(��16λ)
inline void Cmbap::dat2mbreg_hi16bit(u16 reg[1],const float float32) const
{
	//IEEE 754 float ��ʽ,����ת�� http://babbage.cs.qc.cuny.edu/IEEE-754/
	reg[0]=u16(( *(int *)&float32 & 0xFFFF0000)>>16);
}
//��32λ float ������ ת�������� 2�� modbus �Ĵ���(16λ)��(��16λ)
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

/*	��һЩ��Ҫ�����ݴ� stMeter_Run_data �ṹ���и���(ӳ��)��
	reg_tbl �Ĵ������Ա��ȡ��Щ����
	����:	struct stMeter_Run_data meter[MAXMETER]
	���:	u16  reg[0xFFFF+1] �Ĵ�����
	TODO:	Ŀǰ��վ����һ��,����ȫ���������Ĵ�������,�����˷�.
		Ӧ�Ľ�Ч��,��������Ҫ�Ľṹ�����.
	NOTE:	modbus�Ĵ�����ʼ��ַ����0��ʼ����1��ʼ.
*/
int Cmbap::map_dat2reg(u16  reg[0xFFFF+1]
		       ,stMeter_Run_data meter[]
		       ,const struct mb_read_req_pdu request_pdu) const
{
	int i;int j;
	int base;u8 sub=0;// base(�����)+sub(�����)=��ַ
#if 1
	//printf("%d\n",sysConfig->meter_num);
	//for (i=0;i<sysConfig->meter_num;i++) {//(�������б���)
	for (i=0;i<MAXMETER;i++) {//(�������б���)
		base=(i<<8);	//���ֽڱ�ʾ���,�ֱ������ͬ�ı�,��Χ[0,MAXMETER]
		sub=0;				//����,ĳ������ض�����
		//��ʱ���� �����ܵ��� 4*5=20
		//printf("m_iTOU start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_iTOU[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_iTOU[j]);sub++;
			//printf("meter[%d].m_iTOU[%d]=%f sub=%d \n",i,j,meter[i].m_iTOU[j],sub);
		}
		//�����޹����� 4*5=20
		//printf("m_iQR start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){ //
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_iQR[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_iQR[j]);sub++;
			//printf("meter[%d].m_iQR[%d]=%f sub=%d \n",i,j,meter[i].m_iQR[j],sub);
		}
		//������� 2*2*5=20
		//printf("m_iMaxN start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<TOUNUM;j++){ //
			//meter[i].m_iMaxN[j]=123.4F;
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_iMaxN[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_iMaxN[j]);sub++;
			//printf("meter[%d].m_iMaxN[%d]=%f sub=%d \n",i,j,meter[i].m_iMaxN[j],sub);
		}
		//˲ʱ�� 3+3
		//printf("Voltage start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PHASENUM;j++){//��ѹabc
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_wU[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_wU[j]);sub++;
			//printf("meter[%d].m_wU[%d]=%f sub=%d \n",i,j,meter[i].m_wU[j],sub);
		}
		//printf("Current start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PHASENUM;j++){//����abc
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_wI[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_wI[j]);sub++;
			//printf("meter[%d].m_wI[%d]=%f sub=%d \n",i,j,meter[i].m_wI[j],sub);
		}
		//�й� /�޹� /��������|��,a,b,c
		//printf("m_iP start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//�й�
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_iP[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_iP[j]);sub++;
			//printf("meter[%d].m_iP[%d]=%f sub=%d \n",i,j,meter[i].m_iP[j],sub);
		}
		//printf("m_wQ start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//�޹�
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_wQ[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_wQ[j]);sub++;
			//printf("meter[%d].m_wQ[%d]=%f\n",i,j,meter[i].m_wQ[j]);
		}
		//printf("m_wPF start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//��������
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_wPF[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_wPF[j]);sub++;
			//printf("meter[%d].m_wPF[%d]=%f\n",i,j,meter[i].m_wPF[j]);
		}
		//printf("m_wPF start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<4;j++){//����4��32λ�Ĵ���,����
			dat2mbreg_lo16bit(&reg[base+sub],(int)0xFFFFFFFF);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],(int)0xFFFFFFFF);sub++;
			//printf("0xFFFFFFFF=%x %x \n",reg[base+sub-1],reg[base+sub]);
		}
		//�����¼ 4(abc��)*2(����,ʱ��)
		//printf("m_wPBCountstart at  line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_wPBCount[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_wPBCount[j]);sub++;
			//printf("meter[%d].m_wPBCount[%d]=%d sub=%d \n",i,j,meter[i].m_wPBCount[j],sub);

		}
		//������ʱ��
		//printf("m_iPBTotalTime start at line: %d sub=0x%X(%d)\n",__LINE__,sub,sub);
		for(j=0;j<PQCNUM;j++){//
			dat2mbreg_lo16bit(&reg[base+sub],meter[i].m_iPBTotalTime[j]);sub++;
			dat2mbreg_hi16bit(&reg[base+sub],meter[i].m_iPBTotalTime[j]);sub++;
			//printf("meter[%d].m_iPBTotalTime[%d]=%d\n",i,j,meter[i].m_iPBTotalTime[j]);
		}

#if 0		//����:
		dat2mbreg(&reg_tbl[base+sub++],meter[i].m_cF);//short
		dat2mbreg(&reg_tbl[base+sub++],meter[i].m_Ue);
		dat2mbreg(&reg_tbl[base+sub++],meter[i].m_Ie);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].Flag_P3L4,meter[i].FLAG_LP);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].FLAG_TIME,meter[i].m_cMtrnunmb);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_cCommflag,meter[i].m_comflag_first);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_cADDR[0],meter[i].m_cADDR[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_cADDR[2],meter[i].m_cADDR[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_cADDR[4],meter[i].m_cADDR[5]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_com_Pwd[0],meter[i].m_com_Pwd[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_com_Pwd[2],meter[i].m_com_Pwd[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_cPortplan,meter[i].m_cProt);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_time[0],meter[i].m_time[1]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_time[2],meter[i].m_time[3]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_time[4],meter[i].m_time[5]);
		dat2mbreg(&reg_tbl[base+sub++]
			  ,meter[i].m_time[6],0);

#endif
	}//end for
	printf("bytes of one loop ,endline: %d sub=0x%X\n",__LINE__,sub);
#endif

	return 0;
}
/*��Ӧ 0x10����,��ֵ�ӼĴ�����ֵ���ṹ�����
	Ԥ���ı���(��չ)����
*/
int Cmbap::map_reg2dat(u16  reg_tbl[0xFFFF+1]
		       ,stMeter_Run_data meter[]
		       ,const struct mb_write_req_pdu request_pdu) const
{
	int i;int addr;
	u16 start_addr=u16((request_pdu.start_addr_hi<<8)+request_pdu.start_addr_lo);
	u16 reg_quantity=u16((request_pdu.reg_quantity_hi<<8)+request_pdu.reg_quantity_lo);
	unsigned char meter_no=u8((start_addr & 0xFF00)>>8);
	unsigned char sub_id=u8((start_addr & 0xFF));
	u16 end_addr=u16(start_addr+reg_quantity-1);
	unsigned char meter_no_e=u8((end_addr & 0xFF00)>>8);
	meter[0].Flag_Meter=0x87654321;
	printf("start_addr=0x%X meter_no=0x%X sub_id=0x%X end_addr=0x%X meter_no_e=%X"
	       ,start_addr,meter_no,sub_id,end_addr,meter_no_e);
	i=meter_no; //ѭ�� ���
	addr=(i<<8);
	switch(sub_id){ //ÿ����� �ӼĴ�����ַ
begin:case 0x00:dat2mbreg_lo16bit(&reg_tbl[addr+0x00],meter[i].Flag_Meter,1);
		if((addr + 0x00) >= end_addr) goto OK;
	case 0x01:dat2mbreg_hi16bit(&reg_tbl[addr+0x01],meter[i].Flag_Meter,1);
		if((addr + 0x01) >= end_addr) goto OK;
	case 0x02:dat2mbreg_lo16bit(&reg_tbl[addr+0x02],meter[i].Flag_TOU,1);
		if((addr + 0x02) >= end_addr) goto OK;
	case 0x03:dat2mbreg_hi16bit(&reg_tbl[addr+0x03],meter[i].Flag_TOU,1);
		if((addr + 0x03) >= end_addr) goto OK;
	}
	i++;
	addr=(i<<8);
	goto begin;
OK:
	printf("meterData[1].Flag_Meter=0x%X meterData[0].Flag_TOU=%d \n"
	       ,meter[1].Flag_Meter,meter[0].Flag_TOU);
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
