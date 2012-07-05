/*
 * This file is part of tcpflow by Jeremy Elson <jelson@circlemud.org>
 * Initial Release: 7 April 1999.
 *
 * This source code is under the GNU Public License (GPL).  See
 * LICENSE for details.
 *
 */

#include "tcpflow.h"

static char *debug_prefix = NULL;
extern int max_desired_fds;

#define BUFSIZE 1024


/*************************************************************************/



/*
 * Remember our program name and process ID so we can use them later
 * for printing debug messages
 */
void init_debug(char *argv[])
{
    debug_prefix = (char *)calloc(sizeof(char), strlen(argv[0]) + 16);
    if(debug_prefix==0) die("malloc failed");
    sprintf(debug_prefix, "%s[%d]", argv[0], (int) getpid());
}


/*
 * Print a debugging message, given a va_list
 */
void print_debug_message(const char *fmt, va_list ap)
{
    /* print debug prefix */
    fprintf(stderr, "%s: ", debug_prefix);

    /* print the var-arg buffer passed to us */
    vfprintf(stderr, fmt, ap);

    /* add newline */
    fprintf(stderr, "\n");
    (void) fflush(stderr);
}

/* Print a debugging or informational message */
void debug_real(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    print_debug_message(fmt, ap);
    va_end(ap);
}
  

/* Print a debugging or informatioal message, then exit  */
void die(const char *fmt, ...) __attribute__ ((__noreturn__));
void die(const char *fmt, ...) 
{
    va_list ap;

    va_start(ap, fmt);
    print_debug_message(fmt, ap);
    exit(1);
}

/* An attempt at making signal() portable.
 *
 * If we detect sigaction, use that; 
 * otherwise if we have setsig, use that;
 * otherwise, cross our fingers and hope for the best using plain old signal().
 *
 * Our first choice is sigaction (sigaction() is POSIX; signal() is
 * not.)  Taken from Stevens' _Advanced Programming in the UNIX Environment_.
 *
 * 10/6/08 - slg - removed RETSIGTYPE, since it hasn't been needed to 15 years
 */
void (*portable_signal(int signo, void (*func)(int)))(int)
{
#if defined(HAVE_SIGACTION)
    struct sigaction act, oact;

    memset(&act, 0, sizeof(act));
    memset(&oact, 0, sizeof(oact));
    act.sa_handler = func;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (sigaction(signo, &act, &oact) < 0) return (SIG_ERR);
    return (oact.sa_handler);
#elif defined(HAVE_SIGSET)
    return sigset(signo, func);
#else
    return signal(signo, func);
#endif /* HAVE_SIGACTION, HAVE_SIGSET */
}
