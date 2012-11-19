/* ����opc�������Ĵ����� 1.csv ���ڵ��� Ϊ libmbap.so ��������
  ע���ı��������Ϊ GB2312 GB18030֮��windowsĬ�ϵı���,utf-8����
*/
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void mtr(int mtrno);
void base(int mtrno);
void allph(int mtrno);
void instant(int mtrno);
using namespace std;
int dir=2;//���� 2��
int p=2;//�й��޹�
int chan=5;//�ܼ��ƽ�� 5��
int ph=4;//1234����
int cvph=3;//������ѹ3��
FILE* fd;
//������:
int mtr_no=40;//�� ����
int main(int argc,char *argv[])
{
	if(argc==2){
		mtr_no=atol(argv[1]);
	}
	sprintf(argv[0],"asdasd");
	char filename[255];
	sprintf(filename,"./base-%dmeters.csv",mtr_no);
	fd=fopen(filename, "w");
	fprintf(fd,"Tag Name,Address,Data Type,Respect Data Type,"
		"Client Access,Scan Rate,Scaling,Raw Low,Raw High,"
		"Scaled Low,Scaled High,Scaled Data Type,Clamp Low,Clamp "
		"High,Eng Units,Description,Negate Value\r\n");
	int i;
	for(i=0; i<mtr_no; i++) {
		mtr(i);
	}
	fclose(fd);
	while(1);
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
//#define CON (k==0 || ((mtrno==4 || mtrno==15 ||mtrno==16 || (mtrno>=24 && mtrno <=34)) && k!=3))
#define CON (k==0)
	int m=0;
	int i;
	int j;
	int k;
	for(j=0; j<p; j++) {
		for(i=0; i<dir; i++) {
//			if(mtrno==4 || mtrno==15 ||mtrno==16
//					|| (mtrno>=24 && mtrno <=34)
//					){
//				chan=5;
//			}else{
//				chan=1;
//			}
			for (k=0; k<chan; k++) {
				if(CON)fprintf(fd,"Meter %d.1Basic.",mtrno);
				if(i==0) {
					if(CON)fprintf(fd,"Forward");
				} else {
					if(CON)fprintf(fd,"Reverse");
				}
				if(j==0) {
					if(CON)fprintf(fd," Active power");
				} else {
					if(CON)fprintf(fd," Reactive power");
				}
				switch(k) {
				case 0:
					if(CON)fprintf(fd,"-1 Total");
					break;
				case 1:
					if(CON)fprintf(fd,"-2 Tip");
					break;
				case 2:
					if(CON)fprintf(fd,"-3 Peak");
					break;
				case 3:
					if(CON)fprintf(fd,"-4 Flat");
					break;
				case 4:
					if(CON)fprintf(fd,"-5 Valley");
					break;
				default:
					break;
				}
				if(CON){
				fprintf(fd,",%5d,Float,1,RO,100\r\n"
					,400000+256*mtrno+m*2+1);
				}
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
			fprintf(fd,"Meter %d.2pha.",mtrno);
			switch(j) {
			case 0:
				fprintf(fd,"1");
				break;
			case 1:
				fprintf(fd,"2");
				break;
			case 2:
				fprintf(fd,"3");
				break;
			case 3:
				fprintf(fd,"4");
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
			fprintf(fd,"Meter %d.Instantaneous.",mtrno);
			if(j==0) {
				fprintf(fd,"Voltag-");
			} else {
				fprintf(fd,"Current-");
			}
			switch(i) {
			case 0:
				fprintf(fd,"A Phase");
				break;
			case 1:
				fprintf(fd,"B Phase");
				break;
			case 2:
				fprintf(fd,"C Phase");
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
			fprintf(fd,"Meter %d.Instantaneous.",mtrno);
			switch(i) {
			case 0:
				fprintf(fd,"Active Power");
				break;
			case 1:
				fprintf(fd,"Reactive power");
				break;
			case 2:
				fprintf(fd,"Power Factor");
				break;
			default:
				break;
			}
			switch(j) {
			case 0:
				fprintf(fd,"-Total");
				break;
			case 1:
				fprintf(fd,"-A Phase");
				break;
			case 2:
				fprintf(fd,"-B Phase");
				break;
			case 3:
				fprintf(fd,"-C Phase");
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
