#! /bin/bash

CUR_DIR=/home/amax/projects/serverless_nfv/framework

echo $CUR_DIR
INSTALL_LIBS_DIR=$CUR_DIR/../target/libs

make_option=$1

function make_dpdk() {
    LIBS_DIR=$CUR_DIR/../libs/dpdk
    cd $LIBS_DIR
    make
    make install
}

function make_gateway() {
    cd $CUR_DIR/gateway_rtc
#    cd $CUR_DIR/gateway_pipeline
    make clean
    make
    if [ $? != 0 ]; then
        exit 1
    fi
}

function make_executor() {
    cd $CUR_DIR/executor
    make clean
    make
    if [ $? != 0 ]; then
        exit 1
    fi
}

function make_sandbox() {
    cd $CUR_DIR/sandbox
    make clean
    make
    if [ $? != 0 ]; then
        exit 1
    fi
}

function make_framework() {
    cd $CUR_DIR
    make clean
    make
}

function do_make_gateway() {
    make_dpdk
    if [ $? == 0 ];
    then
        echo 'DPDK compiled successfully!'
    else
        echo 'DPDK compiled failed!'
        exit 2
    fi

    make_gateway
    if [ $? == 0 ];
    then
        echo 'Gateway compiled successfully!'
    else
        echo 'Gateway compiled failed!'
        exit 2
    fi
}

function do_make_executor() {
    make_dpdk
    if [ $? == 0 ];
    then
        echo 'DPDK compiled successfully!'
    else
        echo 'DPDK compiled failed!'
        exit 2
    fi

    make_executor
    if [ $? == 0 ];
    then
        echo 'Exector compiled successfully!'
    else
        echo 'Exector compiled failed!'
        exit 2
    fi
}

function do_make_sandbox() {
    make_sandbox
    if [ $? == 0 ];
    then
        echo 'Sandbox compiled successfully!'
    else
        echo 'Sandbox compiled failed!'
        exit 2
    fi
}

if [ "$make_option" == 'gateway' ]
then
    echo 'Making for gateway'
    do_make_gateway
elif [ "$make_option" == 'executor' ]
then
    echo 'Making for executor'
    do_make_executor
elif [ "$make_option" == 'sandbox' ]
then
    echo 'Making for sandbox'
    do_make_sandbox
else
    echo 'Invalid making option'
fi