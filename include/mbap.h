/*	filename: mbap.h -> libmbap.so
	modbus�ļĴ���Ϊ16λ,�Ұ��ȸ��ֽ� ����ֽڴ���
������:
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
//��ʾmbap����Ϣ��ǰ׺.��������ʾ�в鿴�ÿ���ص���Ϣ.
#define MB_PERFIX "[libmbap]"
#define MB_ERR_PERFIX "[libmbap]ERR:"
extern "C" CProtocol *CreateCProto_Cmbap(void);
//libmbap ����Ĵ�����Ϣ

//extern stMeter_Run_data m_meterData[MAXMETER];
//mbap��Լ
class Cmbap :public CProtocol
{
public:
	void SendProc(void);
	Cmbap();
	~Cmbap();
	void m_BroadcastTime(void);
	int ReciProc(void);
	//virtual int Init(struct stPortConfig *tmp_portcfg);
	/************************** ��Ա���� ****************************/
private:
	//����
	struct mbap_head req_mbap;//����ͷ
	struct mb_read_req_pdu read_req_pdu;//��������
	struct mb_write_req_pdu write_req_pdu;//д������
	//��Ӧ
	struct mbap_head rsp_mbap;//��Ӧͷ
	struct mb_read_rsp_pdu read_rsp_pdu;//����Ӧ��-ͷ
	struct mb_write_rsp_pdu write_rsp_pdu;//д��Ӧ��-ͷ
	rsp_dat ppdu_dat[255];//ָ��(��Ӧ��-���ݲ���)��ָ��
	struct mb_excep_rsp_pdu excep_rsp_pdu;//�쳣��Ӧ��-ͷ
	//���мĴ����� 16λÿ�� ��0xFFFF��
	u16 reg_table[0xFFFF];
	/************************** ��Ա���� ****************************/
private://������֤
	bool verify_msg(u8* m_recvBuf,unsigned short len) const;
	bool verify_mbap(const mbap_head request_mbap) const;
	bool verify_req_pdu(const struct  mb_read_req_pdu request_pdu,
			    u8 &errcode);
	bool verify_funcode( u8 funcode)const;
	bool verify_reg_addr(const struct mb_read_req_pdu request_pdu
			     ,int &start_addr,int &end_addr)const;
	bool verify_reg_quantity(const struct mb_read_req_pdu request_pdu
				 ,int &reg_quantity)const;
	int make_excep_msg(struct mbap_head &respond_mbap,
			struct mb_excep_rsp_pdu excep_respond_pdu,
			    u8 func_code, u8 exception_code);
	int make_read_msg( const struct mbap_head request_mbap
		      ,const struct mb_read_req_pdu read_req_pdu
		      ,struct mbap_head &rsp_mbap
		      ,struct mb_read_rsp_pdu &respond_pdu
		      ,u8 pdu_dat[]);//�������ر���
	//��վ Response
	int send_excep_response(void);
	int send_read_response(void) ;
private://���ִ�ӡ:	mbapͷ, ����pdu
	void print_mbap( const mbap_head mbap)  const ;
	void print_req_pdu(const mb_read_req_pdu request_pdu) const;//����
	//		������Ӧ1 / ��Ӧ2
	void print_rsp_pdu(const mb_read_rsp_pdu excep_respond_pdu)const;//��Ӧ1
	void print_pdu_dat(const u8 pdu_dat[],u8 bytecount)const;//��Ӧ1
	void print_excep_rsp_pdu(const mb_excep_rsp_pdu excep_respond_pdu)const;//��Ӧ2
private://ʵ�ú���
	void dat2mbreg(u16 reg[2],const unsigned int dat32);
	void dat2mbreg(u16 reg[2],const signed int  dat32);
	void dat2mbreg(u16 reg[2],const float float32);
	void dat2mbreg(u16 reg[1],const short dat16);
	void dat2mbreg(u16 reg[1],const char high_byte,const char low_byte);
	int map_dat2reg(u16 reg_tbl[0xFFFF],stMeter_Run_data meterData[]);
};
#endif //__MBAP_H__
