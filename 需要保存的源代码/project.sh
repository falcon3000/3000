#!/bin/bash

time=$(date +%Y%m%d%H%M%S)
echo "===================检测开始时间："$time"==================="

#project检测
project=`ps -ef | grep dsnv_new.py | grep -v grep | awk '{print $2}'`  # 判断进程是否开启（main.py改为自己的启动程序名）
if [  -z "$project" ]
then
    cd /sys/class/gpio
    echo '111111' | sudo chmod u=rwx export
    echo '111111' | sudo chmod u=rwx unexport
    echo "#project未启动,执行启动命令"
    cd /opt/nvidia/deepstream/deepstream/sources/deepstream_python_apps/apps/customer/ # 进入启动程序main.py的目录下
    # 启动进程并将打印日志写入nohup.log，nohup结合>main.log 2>&1 &为后台启动，并将日志写入main.log。可自适应修改
    nohup /usr/bin/python3 dsnv_new.py rtsp://admin:a123456789@192.168.20.64/Streaming/Channels/101 rtsp://admin:a123456789@192.168.1.64/Streaming/Channels/101 2>&1 &
else
    echo "#project正常运行"
fi

echo "=========================================================="
