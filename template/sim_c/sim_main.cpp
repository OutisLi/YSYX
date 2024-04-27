#include "verilated.h"
#include "verilated_vcd_c.h"
#include "Vtop.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

VerilatedContext *contextp = NULL;
VerilatedVcdC *vcd = NULL;

static Vtop *top;

void step_and_dump_wave()
{
    top->eval();
    contextp->timeInc(1);
    vcd->dump(contextp->time());
}
void sim_init(int argc, char **argv)
{
    contextp = new VerilatedContext;
    vcd = new VerilatedVcdC;
    top = new Vtop{contextp};
    Verilated::commandArgs(argc, argv);
    contextp->traceEverOn(true);
    contextp->commandArgs(argc, argv);
    top->trace(vcd, 5);
    vcd->open("waveform.vcd");
}

void sim_exit()
{
    step_and_dump_wave();
    vcd->close();
}

int main(int argc, char **argv)
{
    sim_init(argc, argv);

    top->io_sel = 0;
    top->io_a = 0;
    top->io_b = 0;
    step_and_dump_wave(); // 将s，a和b均初始化为“0”
    top->io_b = 1;
    step_and_dump_wave(); // 将b改为“1”，s和a的值不变，继续保持“0”，
    top->io_a = 1;
    top->io_b = 0;
    step_and_dump_wave(); // 将a，b分别改为“1”和“0”，s的值不变，
    top->io_b = 1;
    step_and_dump_wave(); // 将b改为“1”，s和a的值不变，维持10个时间单位

    top->io_sel = 1;
    top->io_a = 0;
    top->io_b = 0;
    step_and_dump_wave(); // 将s，a，b分别变为“1,0,0”，维持10个时间单位
    top->io_b = 1;
    step_and_dump_wave();
    top->io_a = 1;
    top->io_b = 0;
    step_and_dump_wave();
    top->io_b = 1;
    step_and_dump_wave();

    sim_exit();
    delete top;
    delete contextp;
    delete vcd;
    return 0;
}