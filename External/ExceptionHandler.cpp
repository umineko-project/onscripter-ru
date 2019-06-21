/*
 * Copyright (c) 2003, Brian Alliet. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS `AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef IOS_LEGACY_HACKS
// This is an old hack used for iOS 8 and 9 to avoid older devices
// hanging all the time due to infinite EXC_RESOURCE warnings causing
// crashreport generation.
// Note: exc_server is not a public API (obviously).

#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/thread_status.h>
#include <mach/exception.h>
#include <mach/task.h>
#include <pthread.h>

#include "External/Compatibility.hpp"

extern "C" {
	/* These are not defined in any header, although they are "documented" */
	
	boolean_t exc_server(mach_msg_header_t*, mach_msg_header_t*);

	kern_return_t exception_raise(mach_port_t, mach_port_t, mach_port_t,
										 exception_type_t, exception_data_t,
										 mach_msg_type_number_t);

	kern_return_t exception_raise_state(
		mach_port_t, mach_port_t, mach_port_t, exception_type_t, exception_data_t,
		mach_msg_type_number_t, thread_state_flavor_t*, thread_state_t,
		mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t*);

	kern_return_t exception_raise_state_identity(
		mach_port_t, mach_port_t, mach_port_t, exception_type_t, exception_data_t,
		mach_msg_type_number_t, thread_state_flavor_t*, thread_state_t,
		mach_msg_type_number_t, thread_state_t, mach_msg_type_number_t*);
	
	/* These are methods we should export to get the exceptions */
	kern_return_t catch_exception_raise(mach_port_t exception_port,
										mach_port_t thread, mach_port_t task,
										exception_type_t exception,
										exception_data_t code,
										mach_msg_type_number_t code_count);
	
	kern_return_t catch_exception_raise_state(mach_port_name_t exception_port, int exception, exception_data_t code,
											  mach_msg_type_number_t codeCnt, int flavor, thread_state_t old_state,
											  int old_stateCnt, thread_state_t new_state, int new_stateCnt);
	
	kern_return_t catch_exception_raise_state_identity(mach_port_name_t exception_port, mach_port_t thread, mach_port_t task,
													   int exception, exception_data_t code, mach_msg_type_number_t codeCnt,
													   int flavor, thread_state_t old_state, int old_stateCnt,
													   thread_state_t new_state, int new_stateCnt);
}

constexpr size_t MAX_EXCEPTION_PORTS = 16;

static struct {
    mach_msg_type_number_t count;
    exception_mask_t       masks[MAX_EXCEPTION_PORTS];
    exception_handler_t    ports[MAX_EXCEPTION_PORTS];
    exception_behavior_t   behaviors[MAX_EXCEPTION_PORTS];
    thread_state_flavor_t  flavors[MAX_EXCEPTION_PORTS];
} old_exc_ports;

static mach_port_t exception_port;

/* The thread that will handle Mach exceptions.
 * It just runs in a loop waiting for EXC_RESOURCE. */
static void *exc_thread(void*  /*junk*/) {
    mach_msg_return_t r;
    /* These two structures contain some private kernel data. We don't need to
     access any of it so we don't bother defining a proper struct. The
     correct definitions are in the xnu source code. */
    struct {
        mach_msg_header_t head;
        char data[256];
    } reply;

    struct {
        mach_msg_header_t head;
        mach_msg_body_t msgh_body;
        char data[1024];
    } msg;

    while (true) {
        r = mach_msg(&msg.head, MACH_RCV_MSG | MACH_RCV_LARGE, 0, sizeof(msg),
                     exception_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

        if (r != MACH_MSG_SUCCESS) {
            fprintf(stderr, "[exception_handle] r != MACH_MSG_SUCCESS, protection will be disabled (instead of terminating process)\n");
            return nullptr;
        }

        /* Handle the message (calls catch_exception_raise) */
        if (!exc_server(&msg.head, &reply.head)) {
            fprintf(stderr, "[exception_handle] exc_server = 0; protection will be disabled\n");
            return nullptr;
        }

        /* Send the reply */
        r = mach_msg(&reply.head, MACH_SEND_MSG, reply.head.msgh_size, 0,
                     MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (r != MACH_MSG_SUCCESS) {
            fprintf(stderr, "[exception_handle] 2: r != MACH_MSG_SUCCESS, protection will be disabled (instead of terminating process)\n");
            return nullptr;
        }
    }
}

/* Just setup Mach ports and start a thread to handle exceptions. */
CONSTRUCTOR exceptionHandler() {
    kern_return_t r;
    mach_port_t me;
    pthread_t thread;
    pthread_attr_t attr;
    exception_mask_t mask;

    me = mach_task_self();
    r = mach_port_allocate(me, MACH_PORT_RIGHT_RECEIVE, &exception_port);
    if (r != MACH_MSG_SUCCESS) {
        fprintf(stderr, "[exception_handle] can't alloc mach port!\n");
        return;
    }

    r = mach_port_insert_right(me, exception_port, exception_port,
                               MACH_MSG_TYPE_MAKE_SEND);
    if (r != MACH_MSG_SUCCESS) {
        fprintf(stderr, "[exception_handle] can't mach_port_insert_right!\n");
        return;
    }

    /* for others see mach/exception_types.h */
    mask = EXC_MASK_RESOURCE;

    /* get the old exception ports */
    r = task_get_exception_ports(me, mask, old_exc_ports.masks,
                                 &old_exc_ports.count, old_exc_ports.ports,
                                 old_exc_ports.behaviors, old_exc_ports.flavors);
    if (r != MACH_MSG_SUCCESS) {
        fprintf(stderr, "[exception_handle] can't task_get_exception_ports!\n");
        return;
    }

    /* set the new exception ports */
    r = task_set_exception_ports(me, mask, exception_port, EXCEPTION_DEFAULT,
                                 MACHINE_THREAD_STATE);
    if (r != MACH_MSG_SUCCESS) {
        fprintf(stderr, "[exception_handle] can't task_set_exception_ports!\n");
        return;
    }

    if (pthread_attr_init(&attr) != 0) {
        fprintf(stderr, "[exception_handle] can't setup thread...\n");
        return;
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, "[exception_handle] can't setup thread..\n");
        return;
    }
    if (pthread_create(&thread, &attr, exc_thread, nullptr) != 0) {
        fprintf(stderr, "[exception_handle] can't spawn thread. rip\n");
        return;
    }
    pthread_attr_destroy(&attr);
}

/* The source code for Apple's GDB was used as a reference for the exception
   forwarding code. This code is similar to be GDB code only because there is
   only one way to do it. */
/* also, this code should never be reached */
static kern_return_t forward_exception(mach_port_t thread, mach_port_t task,
                                       exception_type_t exception,
                                       exception_data_t data,
                                       mach_msg_type_number_t data_count) {
    mach_msg_type_number_t i;
    kern_return_t r;
    mach_port_t port;
    exception_behavior_t behavior;
    thread_state_flavor_t flavor;

    thread_state_data_t thread_state;
    mach_msg_type_number_t thread_state_count = THREAD_STATE_MAX;

    for (i = 0; i < old_exc_ports.count; i++)
        if (old_exc_ports.masks[i] & (1 << exception))
            break;
    if (i == old_exc_ports.count) {
        fprintf(stderr, "[exception_handle] no handler for exception?\n");
        abort(); // this might actually be an error condition.
    }

    port = old_exc_ports.ports[i];
    behavior = old_exc_ports.behaviors[i];
    flavor = old_exc_ports.flavors[i];

    if (behavior != EXCEPTION_DEFAULT) {
        r = thread_get_state(thread, flavor, thread_state, &thread_state_count);
        if (r != KERN_SUCCESS) {
            fprintf(stderr, "[exception_handle] can't get thread state...\n");
            return KERN_FAILURE;
        }
    }

    switch (behavior) {
    case EXCEPTION_DEFAULT:
        r = exception_raise(port, thread, task, exception, data, data_count);
        break;
    case EXCEPTION_STATE:
        r = exception_raise_state(port, thread, task, exception, data, data_count,
                                  &flavor, thread_state, thread_state_count,
                                  thread_state, &thread_state_count);
        break;
    case EXCEPTION_STATE_IDENTITY:
        r = exception_raise_state_identity(
            port, thread, task, exception, data, data_count, &flavor,
            thread_state, thread_state_count, thread_state, &thread_state_count);
        break;
    default:
        r = KERN_FAILURE; /* make gcc happy */
        break;
    }

    if (behavior != EXCEPTION_DEFAULT) {
        r = thread_set_state(thread, flavor, thread_state, thread_state_count);
        if (r != KERN_SUCCESS) {
            fprintf(stderr, "[exception_handle] thread_set_state failed in forward_exception\n");
        }
    }

    return r;
}

USEDSYM EXPORT kern_return_t catch_exception_raise(mach_port_t  /*exception_port*/,
												   mach_port_t thread, mach_port_t task,
												   exception_type_t exception,
												   exception_data_t code,
												   mach_msg_type_number_t code_count) {
	/* we should never get anything that isn't EXC_RESOURCE, but just in case */
	if (exception != EXC_RESOURCE) {
		/* We aren't interested, pass it on to the old handler */
		fprintf(stderr, "[exception_handle] Exception: 0x%x Code: 0x%x 0x%x in catch....\n", exception,
				code_count > 0 ? code[0] : -1, code_count > 1 ? code[1] : -1);
		return forward_exception(thread, task, exception, code, code_count);
	}

	/*thread_state_flavor_t flavor = ARM_EXCEPTION_STATE;
	mach_msg_type_number_t exc_state_count = ARM_EXCEPTION_STATE_COUNT;
	arm_exception_state_t exc_state;
	kern_return_t r = thread_get_state(thread, flavor, (thread_state_t)&exc_state, &exc_state_count);
	if (r != KERN_SUCCESS) {
		fprintf(stderr, "[exception_handle] can't get thread state. this is ok, mostly.\n");
	} else {
		// This is the address that caused the fault
		// what the fuck? __far?
		void *addr = (void *)exc_state.__far;
		fprintf(stderr, "[exception_handle] *** caught EXC_RESOURCE at fault addr %p.\n", addr);
		fprintf(stderr, "[exception_handle] Returning immediately.\n");
	}*/
	
	/* by returning immediately, we prevent a crash report from being
	 * generated, saving a *lot* of time */
	return KERN_SUCCESS;
}

/* assorted wizardry */
USEDSYM EXPORT kern_return_t catch_exception_raise_state(
    mach_port_name_t  /*exception_port*/, int  /*exception*/, exception_data_t  /*code*/,
    mach_msg_type_number_t  /*codeCnt*/, int  /*flavor*/, thread_state_t  /*old_state*/,
    int  /*old_stateCnt*/, thread_state_t  /*new_state*/, int  /*new_stateCnt*/) {
    return KERN_INVALID_ARGUMENT;
}

USEDSYM EXPORT kern_return_t catch_exception_raise_state_identity(
    mach_port_name_t  /*exception_port*/, mach_port_t  /*thread*/, mach_port_t  /*task*/,
    int  /*exception*/, exception_data_t  /*code*/, mach_msg_type_number_t  /*codeCnt*/,
    int  /*flavor*/, thread_state_t  /*old_state*/, int  /*old_stateCnt*/,
    thread_state_t  /*new_state*/, int  /*new_stateCnt*/) {
    return KERN_INVALID_ARGUMENT;
}

#endif
