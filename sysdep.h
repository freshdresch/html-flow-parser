/*
 * This file is part of tcpflow.
 * Originally by Jeremy Elson <jelson@circlemud.org>
 * Substantially revised by Simson Garfinkel <simsong@acm.org>
 *
 * This source code is under the GNU Public License (GPL).  See
 * LICENSE for details.
 *
 */

/*
 * Set up various machine-specific things based on the values determined
 * from configure and conf.h.
 */


/* Standard C headers  *************************************************/

#ifndef __SYSDEP_H__
#define __SYSDEP_H__

#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdarg>
#include <cerrno>

#include <fcntl.h>
#include <assert.h>

/* If we are including inttypes.h, mmake sure __STDC_FORMAT_MACROS is defined */
#ifdef HAVE_INTTYPES_H
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
# include <inttypes.h>
#else
# error Unable to work without inttypes.h!
#endif


#ifdef HAVE_SYS_CDEFS_H
# include <sys/cdefs.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_BITYPES_H
# include<sys/bitypes.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifndef __FAVOR_BSD
#define __FAVOR_BSD
#endif

#ifndef __USE_BSD
#define __USE_BSD
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_NE_IF_VAR_H
#include <net/if_var.h>
#endif

#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif

#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_NETINET_IP_H
# include <netinet/ip.h>
#endif

#ifdef HAVE_NETINET_IP6_H
# include <netinet/ip6.h>
#endif

#ifdef HAVE_NETINET_IF_ETHER_H
# include <netinet/if_ether.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

/* Linux libc5 systems have different names for certain structures.
 * Patch sent by Johnny Tevessen <j.tevessen@gmx.net> */
#if !defined(HAVE_NETINET_IF_ETHER_H) && defined(HAVE_LINUX_IF_ETHER_H)
# include <linux/if_ether.h>
# define ether_header ethhdr
# define ether_type h_proto
# define ETHERTYPE_IP ETH_P_IP
#endif

/*
 * Oracle Enterprise Linux is missing the definition for
 * ETHERTYPE_VLAN
 */
#ifndef ETHERTYPE_VLAN
# define ETHERTYPE_VLAN 0x8100
#endif

#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif


/****************************************************************
 *** pcap.h (very improtant to this program)
 ***/
#if defined(HAVE_LIBPCAP)

/* pcap.h has redundant definitions */
#  ifdef GNUC_HAS_DIAGNOSTIC_PRAGMA
#    pragma GCC diagnostic ignored "-Wredundant-decls"
#  endif

#  ifdef HAVE_PCAP_PCAP_H
#    include <pcap/pcap.h>
#  else
#    include <pcap.h>
#  endif
#else
#  include "pcap_fake.h"
#endif



/****************** Ugly System Dependencies ******************************/

/* We always want to refer to RLIMIT_NOFILE, even if what you actually
 * have is RLIMIT_OFILE */
#ifdef RLIMIT_OFILE
# ifndef RLIMIT_NOFILE
#  define RLIMIT_NOFILE RLIMIT_OFILE
# endif
#endif

/* We always want to refer to OPEN_MAX, even if what you actually have
 * is FOPEN_MAX. */
#ifdef FOPEN_MAX
# ifndef OPEN_MAX
#  define OPEN_MAX FOPEN_MAX
# endif
#endif

/* some systems don't define SEEK_SET... sigh */
#ifndef SEEK_SET
# define SEEK_SET 0
#endif /* SEEK_SET */

#endif /* __SYSDEP_H__ */

