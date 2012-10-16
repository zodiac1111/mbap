/*生成opc服务器寄存器表 1.csv 用于导入 为 libmbap.so 辅助程序
  注意文本编码必须为 GB2312 GB18030之类windows默认的编码,utf-8不行
*/
#include <iostream>
#include <stdio.h>
void mtr(int mtrno);
void base(int mtrno);
void allph(int mtrno);
void instant(int mtrno);
using namespace std;
int dir=2;//方向 2个
int p=2;//有共无功
int chan=5 ;//总尖峰平谷 5类
int ph=4;//1234象限
int cvph=3;//电流电压3项
FILE* fd;
//可配置:
int mtr_no=40;//表 数量
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
	instant(mtrno);//瞬时值
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
				fprintf(fd,"表%d.1基本.",mtrno);
				if(i==0) {
					fprintf(fd,"正向");
				} else {
					fprintf(fd,"反向");
				}
				if(j==0) {
					fprintf(fd,"有功");
				} else {
					fprintf(fd,"无功");
				}
				switch(k) {
				case 0:
					fprintf(fd,"-1总");
					break;
				case 1:
					fprintf(fd,"-2尖");
					break;
				case 2:
					fprintf(fd,"-3峰");
					break;
				case 3:
					fprintf(fd,"-4平");
					break;
				case 4:
					fprintf(fd,"-5谷");
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
			fprintf(fd,"表%d.2象限无功.",mtrno);
			switch(j) {
			case 0:
				fprintf(fd,"一");
				break;
			case 1:
				fprintf(fd,"二");
				break;
			case 2:
				fprintf(fd,"三");
				break;
			case 3:
				fprintf(fd,"四");
				break;
			default:
				break;
			}
			fprintf(fd,"象限-");
			switch(k) {
			case 0:
				fprintf(fd,"1总");
				break;
			case 1:
				fprintf(fd,"2尖");
				break;
			case 2:
				fprintf(fd,"3峰");
				break;
			case 3:
				fprintf(fd,"4平");
				break;
			case 4:
				fprintf(fd,"5谷");
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
	for (j=0; j<2; j++) { //电压 电流
		for(i=0; i<cvph; i++) {
			fprintf(fd,"表%d.4瞬时量.",mtrno);
			if(j==0) {
				fprintf(fd,"电压-");
			} else {
				fprintf(fd,"电流-");
			}
			switch(i) {
			case 0:
				fprintf(fd,"A相");
				break;
			case 1:
				fprintf(fd,"B相");
				break;
			case 2:
				fprintf(fd,"C相");
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
	for(i=0; i<3; i++) { //有功功率 无功功率  功率因数
		for (j=0; j<4; j++) { //总 A B C
			fprintf(fd,"表%d.4瞬时量.",mtrno);
			switch(i) {
			case 0:
				fprintf(fd,"有功功率");
				break;
			case 1:
				fprintf(fd,"无功功率");
				break;
			case 2:
				fprintf(fd,"功率因数");
				break;
			default:
				break;
			}
			switch(j) {
			case 0:
				fprintf(fd,"-总");
				break;
			case 1:
				fprintf(fd,"-A相");
				break;
			case 2:
				fprintf(fd,"-B相");
				break;
			case 3:
				fprintf(fd,"-C相");
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
