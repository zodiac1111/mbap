现场安装人员
==========

##配置
libmbap.so即规约库文件.放在/mnt/nor/lib/目录下面

在规约配置文件(/mnt/nor/conf/protocol_config.txt)中添加一行:

	mbap 1 mbap Cproto_Cmbap



* 终端地址:255(必须且唯一)
* IP地址和端口地址视情况更改.

重启终端.

##调试/排错
终端执行

	msgmon <端口号>

可以查看终端收发的报文.示例见参考报文.

