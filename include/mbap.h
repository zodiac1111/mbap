/*	filename: mbap.h -> libmbap.so
	modbus�ļĴ���Ϊ16λ,�Ұ��ȸ��ֽ� ����ֽڴ���
	���ڶ���16λ������������ int float,��2�ֽ���ǰ.
	ע�����ݸ�ʽ,�Ĵ�����ʽ,����˳��,
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
#define MB_PERFIX "[libmbap]"	//����һ����Ϣ ���������
#define MB_PERFIX_WARN "[libmbap]Warning:"//������ʾ ���Ի�����Ϣ
#define MB_PERFIX_ERR "[libmbap]ERR:"	//������ʾ ������Ϣ
extern "C" CProtocol *CreateCProto_Cmbap(void);
//libmbap ����Ĵ�����Ϣ
//extern stMeter_Run_data m_meterData[MAXMETER];
// ����ѡ��:
//#define DBG_REG_DAT //ǿ�����ýṹ������,���ڼĴ�������(�����紫��)����
#define DBG_SHOW_RECI_MSG//���ն���ʾ���յ� ��Ϣ(����)
#define SHOW_SEND_MSG //���ն���ʾ ���͵� ��Ϣ(����)
#define SHOW_SEND_ERR_MSG //���ն���ʾ ���͵� �쳣 ��Ϣ(����)
//#define READ_DATE_PAND_DBG //��ʾ��䵽�Ĵ�����ֵ
//#define DBG_send_response
//mbap��Լ
class Cmbap :public CProtocol
{
public:
	Cmbap();
	~Cmbap();
	int Init(struct stPortConfig *tmp_portcfg);
	void SendProc(void);
	int ReciProc(void);
	void m_BroadcastTime(void);
	//int Init(struct stPortConfig *tmp_portcfg);
	/************************** ��Ա���� ****************************/
private:
	u8 slave_ID;//��վID
	//����
	struct mbap_head req_mbap;//����ͷ
	struct mb_read_req_pdu read_req_pdu;//��������
	struct mb_write_req_pdu write_req_pdu;//д������
	//��Ӧ
	struct mbap_head rsp_mbap;//��Ӧͷ
	struct mb_read_rsp_pdu read_rsp_pdu;//����Ӧ��-ͷ
	struct mb_write_rsp_pdu write_rsp_pdu;//д��Ӧ��-ͷ
	rsp_dat ppdu_dat[256];//ָ��(��Ӧ��-���ݲ���)��ָ��
	struct mb_excep_rsp_pdu excep_rsp_pdu;//�쳣��Ӧ��-ͷ
	//���мĴ����� 16λÿ�� ��0xFFFF+1��
	u16 reg_table[0xFFFF+1];
	/************************** ��Ա���� ****************************/
private://������֤
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
	//�������ر��� 0x06
	int make_msg( const struct mbap_head request_mbap
		      ,const struct mb_read_req_pdu read_req_pdu
		      ,struct mbap_head &rsp_mbap
		      ,struct mb_read_rsp_pdu &respond_pdu
		      ,u8 pdu_dat[])const;
	//�������ر��� 0x10
	int make_msg( const struct mbap_head request_mbap
		      ,const struct mb_write_req_pdu read_req_pdu
		      ,struct mbap_head &rsp_mbap
		      ,struct mb_write_rsp_pdu  &respond_pdu)const;
	//�����쳣���ر���
	int make_msg_excep(struct mbap_head &mbap,
			   mb_excep_rsp_pdu &excep_pdu,
			   u8 func_code, u8 exception_code)const;
	//�����쳣�ظ�
	int send_excep_response(const struct mbap_head mbap,
				const struct mb_excep_rsp_pdu pdu,
				struct TransReceiveBuf &transBuf )const ;
	//���������ظ�,����
	int send_response(const mbap_head mbap
			  ,const mb_read_rsp_pdu pdu
			  ,const rsp_dat pdu_dat[],
			  struct TransReceiveBuf &transBuf) const;
	int send_response(const struct mbap_head mbap
				 ,const mb_write_rsp_pdu pdu
				 ,TransReceiveBuf &transBuf)const;
private://���ִ�ӡ:	mbapͷ, ����pdu
	void print_mbap( const mbap_head mbap)const;// 0x06 adn 0x10
	void print_req_pdu(const mb_read_req_pdu request_pdu)const;//read����
	void print_req_pdu(const mb_write_req_pdu request_pdu)const;//write x010
	//		������Ӧ1 / ��Ӧ2
	void print_rsp_pdu(const mb_read_rsp_pdu respond_pdu)const;//0x06
	void print_rsp_pdu(const mb_write_rsp_pdu respond_pdu)const;//0x10
	void print_rsp_pdu(const mb_excep_rsp_pdu excep_respond_pdu)const;//�쳣
	void print_pdu_dat(const u8 pdu_dat[],u8 bytecount)const;//0x06/0x10������
private://ʵ�ú��� ����������ת����Ϊ 16λmodbus�Ĵ�������
	void dat2mbreg_hi16bit(u16 reg[1],unsigned int &dat32, int dir) const;
	void dat2mbreg_lo16bit(u16 reg[1],unsigned int &dat32, int dir) const;
	void dat2mbreg_hi16bit(u16 reg[1],const signed int  dat32) const;
	void dat2mbreg_lo16bit(u16 reg[1],const signed int  dat32) const;
	void dat2mbreg_hi16bit(u16 reg[1],const float float32) const;
	void dat2mbreg_lo16bit(u16 reg[1],const float float32) const;
	void dat2mbreg(u16 reg[1],const short dat16) const;
	void dat2mbreg(u16 reg[1],const char high_byte,const char low_byte) const;
	int map_dat2reg(u16 reg_tbl[0xFFFF],stMeter_Run_data meterData[]
			, const mb_read_req_pdu request_pdu)const;
	int map_reg2dat(u16 reg_tbl[]
			       ,stMeter_Run_data meterData[]
			       ,const struct mb_write_req_pdu request_pdu) const;

};
#endif //__MBAP_H__
