//
// Created byhy on 19-3-17.
//

#include "funcbox_0.h"
#include "funcworker_0.h"

int main(int argc, char *argv[]) {

    func_worker_init(argc, argv);

    func_worker_run();

    printf("Funcbox exited\n");
    return 0;
}

