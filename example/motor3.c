/*
 * david austin
 * http://www.embedded.com/design/mcus-processors-and-socs/4006438/Generate-stepper-motor-speed-profiles-in-real-time
 * DECEMBER 30, 2004
 *
 * Demo program for stepper motor control with linear ramps
 * Hardware: PIC18F252, L6219
 *
 * Copyright (c) 2015 Robert Ramey
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include <assert.h>

// ramp state-machine states
enum ramp_state {
    ramp_idle = 0,
    ramp_up = 1,
    ramp_const = 2,
    ramp_down = 3,
};

// ***************************
// 1. Define state variables using custom strong types

// initial setup
enum ramp_state ramp_sts;
step_t motor_position;
step_t m;               // target position
step_t m2;               // midpoint or point where acceleration changes
direction_t d;          // direction of traval -1 or +1

// curent state along travel
step_t i;               // step number
c_t c;                  // 24.8 fixed point delay count increment
ccpr_t ccpr;            // 24.8 fixed point delay count
phase_ix_t phase_ix;    // motor phase index

// ***************************
// 2. Surround all literal values with the "literal" keyword

// Config data to make CCP1&2 generate quadrature sequence on PHASE pins
// Action on CCP match: 8=set+irq; 9=clear+irq
phase_t const ccpPhase[] = {
    literal(0x909),
    literal(0x908),
    literal(0x808),
    literal(0x809)
}; // 00,01,11,10

void current_on(){/* code as needed */}  // motor drive current
void current_off(){/* code as needed */} // reduce to holding value

// ***************************
// 3. Refactor code to make it easier to understand
// and relate to the documentation

bool busy(){
    return ramp_idle != ramp_sts;
}

// set outputs to energize motor coils
void update(ccpr_t ccpr, phase_ix_t phase_ix){
    phase_t phase;           // ccpPhase[phase_ix]
    // energize correct windings
    phase = ccpPhase[phase_ix];
    CCP1CON = phase & literal(0xff); // set CCP action on next match
    CCP2CON = phase >> literal(8);
    // timer value at next CCP match
    CCPR2H = CCPR1H = literal(0xff) & (ccpr >> literal(8));
    CCPR2L = CCPR1L = literal(0xff) & ccpr;
}

// compiler-specific ISR declaration
// ***************************
// 4. Rewrite interrupt handler in a way which mirrors the orginal
// description of the algorithm and minimizes usage of state variable,
// accumulated values, etc.
void interrupt isr_motor_step(void) { // CCP1 match -> step pulse + IRQ
    // *** possible exception
    ++i;
    // *** possible exception
    motor_position += d;
    // calculate next difference in time
    for(;;){
        switch (ramp_sts) {
            case ramp_up: // acceleration
                if (i == m2) {
                    ramp_sts = ramp_down;
                    continue;
                }
                // *** possible exception
                // equation 12
                c -= literal(2) * c / (literal(4) * i + literal(1));
                if(c < C_MIN){
                    c = C_MIN;
                    ramp_sts = ramp_const;
                    // *** possible exception
                    m2 = m - i; // new inflection point
                    continue;
                }
                break;
            case ramp_const: // constant speed
                if(i > m2) {
                    ramp_sts = ramp_down;
                    continue;
                }
                break;
            case ramp_down: // deceleration
                if (i == m) {
                    ramp_sts = ramp_idle;
                    current_off(); // reduce motor current to holding value
                    CCP1IE = literal(0); // disable_interrupts(INT_CCP1);
                    return;
                }
                // *** possible exception
                // equation 14
                c -= literal(2) * c / (literal(4) * (i - m) + literal(1));
                if(c > C0){
                    c = C0;
                }
                break;
        } // switch (ramp_sts)
        break;
    }
    assert(c <= C0 && c >= C_MIN);
    // *** possible exception
    ccpr = literal(0xffffff) & (ccpr + c);
    // *** possible exception
    phase_ix = (phase_ix + d) & literal(3);
    update(ccpr, phase_ix);
} // isr_motor_step()

// set up to drive motor to pos_new (absolute step#)
void motor_run(step_t new_position) {
    if(new_position > motor_position){
        d = literal(1);
        // *** possible exception
        m = new_position - motor_position;
    }
    else
    if(motor_position > new_position){
        d = literal(-1);
        // *** possible exception
        m = motor_position - new_position;
    }
    else{
        d = literal(0);
        m = literal(0);
        ramp_sts = ramp_idle; // start ramp state-machine
        return;
    }

    i = literal(0);
    // *** possible exception
    m2 = (m - i) / 2;

    ramp_sts = ramp_up; // start ramp state-machine

    T1CONbits.TMR1ON = literal(0); // stop timer1;

    current_on(); // current in motor windings

    c = C0;
    ccpr = (TMR1H << literal(8) | TMR1L);
    // *** possible exception
    ccpr += literal(1000);
    // *** possible exception
    ccpr += c;
    phase_ix = d & literal(3);
    update(ccpr, phase_ix);

    CCP1IE = literal(1); // enable_interrupts(INT_CCP1);
    T1CONbits.TMR1ON = literal(1); // restart timer1;
} // motor_run()

void initialize() {
    motor_position = literal(0);
    INTCON = literal(0); // disable_interrupts(GLOBAL);
    CCP1IE = literal(0); // disable_interrupts(INT_CCP1);
    CCP2IE = literal(0); // disable_interrupts(INT_CCP2);
    PORTC = literal(0); // output_c(0);
    TRISC = literal(0); // set_tris_c(0);
    T3CON = literal(0);
    T1CON = literal(0x35);
    INTCON = literal(0xff); // enable_interrupts(GLOBAL);
} // initialize()
