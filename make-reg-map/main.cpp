/*����opc�������Ĵ����� 1.csv ���ڵ��� Ϊ libmbap.so ��������
  ע���ı��������Ϊ GB2312 GB18030֮��windowsĬ�ϵı���,utf-8����
*/
#include <iostream>
#include <stdio.h>
void mtr(int mtrno);
void base(int mtrno);
void allph(int mtrno);
void instant(int mtrno);
using namespace std;
int dir=2;//���� 2��
int p=2;//�й��޹�
int chan=5 ;//�ܼ��ƽ�� 5��
int ph=4;//1234����
int cvph=3;//������ѹ3��
FILE* fd;
//������:
int mtr_no=40;//�� ����
int main()
{
	fd=fopen("./1.csv", "w");
	fprintf(fd,"Tag Name,Address,Data Type,Respect Data Type,"
	        "Client Access,Scan Rate,Scaling,Raw Low,Raw High,"
	        "Scaled Low,Scaled High,Scaled Data Type,Clamp Low,Clamp "
	        "High,Eng Units,Description,Negate Value\r\n");
	int i;
	for(i=0; i<mtr_no; i++) {
		mtr(i);
	}
	fclose(fd);
	//cout << "************ EOF ****************" << endl;
	return 0;
}
void mtr(int mtrno)
{
	base(mtrno);
#if 0
	allph(mtrno);
#endif
	//max_need(mtrno);
#if 0
	instant(mtrno);//˲ʱֵ
#endif
	//bocken(mtrno);
	return;
}
void base(int mtrno)
{
	int m=0;
	int i;
	int j;
	int k;
	for(j=0; j<p; j++) {
		for(i=0; i<dir; i++) {
			for (k=0; k<chan; k++) {
				fprintf(fd,"��%d.1����.",mtrno);
				if(i==0) {
					fprintf(fd,"����");
				} else {
					fprintf(fd,"����");
				}
				if(j==0) {
					fprintf(fd,"�й�");
				} else {
					fprintf(fd,"�޹�");
				}
				switch(k) {
				case 0:
					fprintf(fd,"-1��");
					break;
				case 1:
					fprintf(fd,"-2��");
					break;
				case 2:
					fprintf(fd,"-3��");
					break;
				case 3:
					fprintf(fd,"-4ƽ");
					break;
				case 4:
					fprintf(fd,"-5��");
					break;
				default:
					break;
				}
				fprintf(fd,",%5d,Float,1,RO,100\r\n"
				        ,400000+256*mtrno+m*2+1);
				m++;
			}
		}
	}
}
void allph(int mtrno)
{
	int m=0;
	int j;
	int k;
	for(j=0; j<ph; j++) {
		for (k=0; k<chan; k++) {
			fprintf(fd,"��%d.2�����޹�.",mtrno);
			switch(j) {
			case 0:
				fprintf(fd,"һ");
				break;
			case 1:
				fprintf(fd,"��");
				break;
			case 2:
				fprintf(fd,"��");
				break;
			case 3:
				fprintf(fd,"��");
				break;
			default:
				break;
			}
			fprintf(fd,"����-");
			switch(k) {
			case 0:
				fprintf(fd,"1��");
				break;
			case 1:
				fprintf(fd,"2��");
				break;
			case 2:
				fprintf(fd,"3��");
				break;
			case 3:
				fprintf(fd,"4ƽ");
				break;
			case 4:
				fprintf(fd,"5��");
				break;
			default:
				break;
			}
			fprintf(fd,",%5d,Float,1,RO,100\r\n"
			        ,400000+256*mtrno+m*2+1+40);
			m++;
		}
	}
	return;
}
void instant(int mtrno)
{
	int m=0;
	int i;
	int j;
	for (j=0; j<2; j++) { //��ѹ ����
		for(i=0; i<cvph; i++) {
			fprintf(fd,"��%d.4˲ʱ��.",mtrno);
			if(j==0) {
				fprintf(fd,"��ѹ-");
			} else {
				fprintf(fd,"����-");
			}
			switch(i) {
			case 0:
				fprintf(fd,"A��");
				break;
			case 1:
				fprintf(fd,"B��");
				break;
			case 2:
				fprintf(fd,"C��");
				break;
			default:
				break;
			}
			fprintf(fd,",%5d,Float,1,RO,100\r\n"
			        ,400000+256*mtrno+m*2+1+120);
			m++;
		}
	}
	m=0;
	for(i=0; i<3; i++) { //�й����� �޹�����  ��������
		for (j=0; j<4; j++) { //�� A B C
			fprintf(fd,"��%d.4˲ʱ��.",mtrno);
			switch(i) {
			case 0:
				fprintf(fd,"�й�����");
				break;
			case 1:
				fprintf(fd,"�޹�����");
				break;
			case 2:
				fprintf(fd,"��������");
				break;
			default:
				break;
			}
			switch(j) {
			case 0:
				fprintf(fd,"-��");
				break;
			case 1:
				fprintf(fd,"-A��");
				break;
			case 2:
				fprintf(fd,"-B��");
				break;
			case 3:
				fprintf(fd,"-C��");
				break;
			default:
				break;
			}
			fprintf(fd,",%5d,Float,1,RO,100\r\n"
			        ,400000+256*mtrno+m*2+1+132);
			m++;
		}
	}
	return;
}
