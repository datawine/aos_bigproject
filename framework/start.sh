#! /bin/bash
#./make.sh
if [ $# -ne 1 ]; then
echo "Specify which execution to run, e.g., ./start.sh gateway"
exit 1
fi


program=$1

if [ "$program" == 'gateway' ]; then
    echo "Running Gateway"
    # 010101010101
    sudo ./gateway_rtc/build/app/serverlessnfv_gateway -c 0x1555 -n 4 -w 04:00.0 -w 04:00.1  -- -p 0x03
#    sudo ./gateway_pipeline/build/app/serverlessnfv_gateway -c 0x1555 -n 4 -w 04:00.0 -w 04:00.1  -- -p 0x03
elif [ "$program" == 'executor' ]; then
    echo "Running Executor"
    sudo ./executor/build/app/serverlessnfv_executor -c 0xf000 -n 4 --proc-type=primary --base-virtaddr=0x100000000  -- -p 0x01
#    sudo ./executor/build/app/serverlessnfv_executor -c 0xf1f0 -n 4 --proc-type=primary --base-virtaddr=0x100000000  -- -p 0x01
#    sudo ./executor/build/app/serverlessnfv_executor -c 0x15 -n 4 --proc-type=primary --base-virtaddr=0x100000000  -- -p 0x01
#    sudo ./executor/build/app/serverlessnfv_executor -c 0x2a -n 4 --proc-type=primary --base-virtaddr=0x100000000  -- -p 0x01
elif [ "$program" == 'sandbox' ]; then
    echo "Running Sandbox"
    sudo python sandbox/sandbox_daemon.py
else
    echo "Invalid option"
fi

