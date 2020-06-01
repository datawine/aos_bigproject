#!/usr/bin/env bash

#sudo /home/ubuntu/projects/serverless-nfv/framework/sandbox/sandbox -npu mybox bash
#sudo /home/ubuntu/projects/serverless-nfv/framework/sandbox/sandbox -npu funcbox_0_0 /home/ubuntu/projects/serverless-nfv/funcboxes/funcbox_0/test
sudo /home/ubuntu/projects/serverless-nfv/framework/sandbox/sandbox -npu funcbox_0_0 /home/ubuntu/projects/serverless-nfv/funcboxes/funcbox_0/build/app/funcbox_0 -l 8 -n 4 --proc-type=secondary -- -r Deliver_rx_1_queue -t Deliver_tx_1_queue