#!/bin/bash

# for arg in "$@"
# do
#   case $arg in
#     "start")
#       echo "Starting service..."
#       # 执行启动服务的命令
#       ;;
#     "stop")
#       echo "Stopping service..."
#       # 执行停止服务的命令
#       ;;
#     "restart")
#       echo "Restarting service..."
#       # 执行重启服务的命令
#       ;;
#     *)
#       echo "Invalid argument: $arg"
#       ;;
#   esac
# done
# cd /home/lien/linux/drivers

echo `pwd`
echo "开始执行depmod和modprobe"
depmod
modprobe pwm_JUYAN.ko
echo "执行depmod和modprobe结束"
cd /sys/class/pwm/pwmchip0/
for((i=0; i<=4; i++))
do
        echo `pwd`
        echo $i > export
done
for((i=0; i<=4; i++))
do
        echo 20000 > /sys/class/pwm/pwmchip0/pwm$i/period #设置周期为 20000 纳秒
        echo $[1000 + 2000 * $i] > /sys/class/pwm/pwmchip0/pwm$i/duty_cycle #设置占空比为 10000 纳秒
        echo 1 > /sys/class/pwm/pwmchip0/pwm$i/enable         #使能（ 0 表示禁止）
        # # echo /sys/class/pwm/pwmchip0/pwm$i/period #设置周期为 20000 纳秒
        # # echo $[500 * $i] #设置周期为 20000 纳秒
done
while true
do
    for i in $(seq 1 10)
    do
        # echo $((200 * i)) > /sys/class/pwm/pwmchip0/pwm$i/duty_cycle #设置占空比为 1000 + 2000 * i 纳秒
        echo $((2000 * i)) > /sys/class/pwm/pwmchip0/pwm0/duty_cycle 
        echo $((2000 * i)) > /sys/class/pwm/pwmchip0/pwm1/duty_cycle 
        sleep 0.5
    done

    for i in $(seq 1 10)
    do
        # echo $((200 * i)) > /sys/class/pwm/pwmchip0/pwm$i/duty_cycle #设置占空比为 1000 + 2000 * i 纳秒
        echo $((20000 - 2000 * i)) > /sys/class/pwm/pwmchip0/pwm0/duty_cycle 
        echo $((20000 - 2000 * i)) > /sys/class/pwm/pwmchip0/pwm1/duty_cycle 
        sleep 0.5
    done
done

