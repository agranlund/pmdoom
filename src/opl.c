//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     OPL interface.
//

#include <mint/osbind.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL.h"

#include "isa.h"
#include "opl.h"

static unsigned int opl_base = 0x388;       /* default Adlib port number */
static unsigned int opl_delay_idx = 5;      /* default Adlib index write delay */
static unsigned int opl_delay_reg = 35;     /* default Adlib data write delay */


void OPL_WritePort(opl_port_t port, unsigned int value) {
    isa_writeb(opl_base + port, value);
}

unsigned int OPL_ReadPort(opl_port_t port) {
    return isa_readb(opl_base + port);
}

void OPL_Delay(uint64_t us) {
    isa_delay(us);
}


opl_init_result_t OPL_Init(unsigned int port_base)
{
    if (!isa_init()) {
        return OPL_INIT_NONE;
    }

    opl_base = port_base;
    opl_init_result_t result = OPL_Detect();
    if (result == OPL_INIT_NONE) {
        return OPL_INIT_NONE;
    }

    /* opl3 needs almost no write delay */
    if (result == OPL_INIT_OPL3) {
        opl_delay_idx = 1;
        opl_delay_reg = 1;
    }

    return result;
}

void OPL_Shutdown(void)
{
}

// Higher-level functions, based on the lower-level functions above
// (register write, etc).
unsigned int OPL_ReadStatus(void)
{
    return OPL_ReadPort(OPL_REGISTER_PORT);
}

// Write an OPL register value
void OPL_WriteRegister(int reg, int value)
{
    if (reg & 0x100) {
        OPL_WritePort(OPL_REGISTER_PORT_OPL3, reg - 0x100);
    } else {
        OPL_WritePort(OPL_REGISTER_PORT, reg);
    }
    OPL_Delay(opl_delay_idx);
    OPL_WritePort(OPL_DATA_PORT, value);
    OPL_Delay(opl_delay_reg);
}

// Detect the presence of an OPL chip
opl_init_result_t OPL_Detect(void)
{
    // Reset both timers
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    // Enable interrupts
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);
    // Read status
    int result1 = OPL_ReadStatus();
    // Set timer 1
    OPL_WriteRegister(OPL_REG_TIMER1, 0xff);
    // Start timer 1
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x21);

    OPL_Delay(2000);

    // Read status
    int result2 = OPL_ReadStatus();

    // Reset both timers:
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x60);
    // Enable interrupts:
    OPL_WriteRegister(OPL_REG_TIMER_CTRL, 0x80);

    if (((result1 & 0xe0) == 0x00) && ((result2 & 0xe0) == 0xc0)) {
        if ((result2 & 0x06) == 0x00) {
            return OPL_INIT_OPL3;
        }
        return OPL_INIT_OPL2;
    }
    return OPL_INIT_NONE;
}

// Initialize registers on startup
void OPL_InitRegisters(int opl3)
{
    int r;

    // Initialize level registers
    for (r=OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
        OPL_WriteRegister(r, 0x3f);

    // Initialize other registers
    // These two loops write to registers that actually don't exist,
    // but this is what Doom does ...
    // Similarly, the <= is also intenational.
    for (r=OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
        OPL_WriteRegister(r, 0x00);

    // More registers ...
    for (r=1; r < OPL_REGS_LEVEL; ++r)
        OPL_WriteRegister(r, 0x00);

    // Re-initialize the low registers:
    // Reset both timers and enable interrupts:
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,      0x60);
    OPL_WriteRegister(OPL_REG_TIMER_CTRL,      0x80);

    // "Allow FM chips to control the waveform of each operator":
    OPL_WriteRegister(OPL_REG_WAVEFORM_ENABLE, 0x20);

    if (opl3)
    {
        OPL_WriteRegister(OPL_REG_NEW, 0x01);

        // Initialize level registers
        for (r=OPL_REGS_LEVEL; r <= OPL_REGS_LEVEL + OPL_NUM_OPERATORS; ++r)
            OPL_WriteRegister(r | 0x100, 0x3f);

        // Initialize other registers
        // These two loops write to registers that actually don't exist,
        // but this is what Doom does ...
        // Similarly, the <= is also intenational.
        for (r=OPL_REGS_ATTACK; r <= OPL_REGS_WAVEFORM + OPL_NUM_OPERATORS; ++r)
            OPL_WriteRegister(r | 0x100, 0x00);

        // More registers ...
        for (r=1; r < OPL_REGS_LEVEL; ++r)
            OPL_WriteRegister(r | 0x100, 0x00);
    }

    // Keyboard split point on (?)
    OPL_WriteRegister(OPL_REG_FM_MODE,         0x40);

    if (opl3) {
        OPL_WriteRegister(OPL_REG_NEW, 0x01);
    }
}

void OPL_SetPaused(int paused)
{
}

