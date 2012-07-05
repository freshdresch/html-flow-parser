/* 
 *
 * Notice: this file is a modified version of tcpdemux.h from the true tcpflow.
 * I wanted to simplify it for my moderately narrow purposes.
 * The original license may be found below. -Adam Drescher
 * ---------------------------------------------------------------------
 * This file is a part of tcpflow by Simson Garfinkel <simsong@acm.org>.
 * Originally by Jeremy Elson <jelson@circlemud.org>.
 *
 * This source code is under the GNU Public License (GPL).  See
 * LICENSE for details.
 *
 */

#ifndef __TCPFLOW_H__
#define __TCPFLOW_H__

#include "config.h"
#include "sysdep.h"


/* required per C++ standard */
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif


/**************************** Constants ***********************************/

/* #define DEFAULT_DEBUG_LEVEL 1 */

#define MAX_FD_GUESS        64
#define NUM_RESERVED_FDS    5     /* number of FDs to set aside */
#define SNAPLEN             65536 /* largest possible MTU we'll see */

#include <iostream>

#include "tcpdemux.h"
  
/***************************** Macros *************************************/

#define IS_SET(vector, flag) ((vector) & (flag))
#define SET_BIT(vector, flag) ((vector) |= (flag))

/************************* Function prototypes ****************************/

/* datalink.cpp - callback for libpcap */
pcap_handler find_handler(int datalink_type, const char *device); // callback for pcap

/* flow.cpp - handles the flow database */
void flow_close_all();

/* main.cpp - CLI */
extern const char *progname;
extern u_int min_skip;

#ifdef HAVE_PTHREAD
#include <semaphore.h>
extern sem_t *semlock;
#endif

/* util.c - utility functions */
void init_debug(char *argv[]);
void (*portable_signal(int signo, void (*func)(int)))(int);
void debug_real(const char *fmt, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 1, 2)))
#endif
;
void die(const char *fmt, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 1, 2)))
#endif
;

#endif /* __TCPFLOW_H__ */
