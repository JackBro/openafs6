/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define _POSIX_PTHREAD_SEMANTICS
#include <afs/param.h>
#include <assert.h>
#include <stdio.h>
#ifndef  AFS_NT40_ENV
#include <signal.h>
#include <unistd.h>
#else
#include <afs/procmgmt.h>
#endif
#include <pthread.h>

#include "pthread_nosigs.h"

static pthread_t softsig_tid;
static struct {
    void (*handler) (int);
    int pending;
    int fatal;
    int inited;
} softsig_sigs[NSIG];

static void *
softsig_thread(void *arg)
{
    sigset_t ss, os;
    int i;

    sigemptyset(&ss);
    /* get the list of signals _not_ blocked by AFS_SIGSET_CLEAR() */
    pthread_sigmask(SIG_BLOCK, &ss, &os);
    pthread_sigmask(SIG_SETMASK, &os, NULL);
    sigaddset(&ss, SIGUSR1);
    for (i = 0; i < NSIG; i++) {
	if (!sigismember(&os, i) && i != SIGSTOP && i != SIGKILL) {
	    sigaddset(&ss, i);
	    softsig_sigs[i].fatal = 1;
	}
    }

    while (1) {
	void (*h) (int);
	int sigw;

	h = NULL;

	for (i = 0; i < NSIG; i++) {
	    if (softsig_sigs[i].handler && !softsig_sigs[i].inited) {
		sigaddset(&ss, i);
		softsig_sigs[i].inited = 1;
	    }
	    if (softsig_sigs[i].pending) {
		softsig_sigs[i].pending = 0;
		h = softsig_sigs[i].handler;
		break;
	    }
	}
	if (i == NSIG) {
	    sigwait(&ss, &sigw);
	    if (sigw != SIGUSR1) {
		if (softsig_sigs[sigw].fatal)
		    exit(0);
		softsig_sigs[sigw].pending = 1;
	    }
	} else if (h)
	    h(i);
    }
}

void
softsig_init()
{
    int rc;
    AFS_SIGSET_DECL;
    AFS_SIGSET_CLEAR();
    rc = pthread_create(&softsig_tid, NULL, &softsig_thread, NULL);
    assert(0 == rc);
    AFS_SIGSET_RESTORE();
}

static void
softsig_handler(int signo)
{
    signal(signo, softsig_handler);
    softsig_sigs[signo].pending = 1;
    pthread_kill(softsig_tid, SIGUSR1);
}

void
softsig_signal(int signo, void (*handler) (int))
{
    softsig_sigs[signo].handler = handler;
    softsig_sigs[signo].inited = 0;
    signal(signo, softsig_handler);
    pthread_kill(softsig_tid, SIGUSR1);
}

#if defined(TEST)
static void
print_foo(int signo)
{
    printf("foo, signo = %d, tid = %d\n", signo, pthread_self());
}

int
main()
{
    softsig_init();
    softsig_signal(SIGINT, print_foo);
    printf("main is tid %d\n", pthread_self());
    while (1)
	sleep(60);
}
#endif
