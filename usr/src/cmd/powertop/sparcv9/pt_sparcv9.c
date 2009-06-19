/*
 * Copyright 2009, Intel Corporation
 * Copyright 2009, Sun Microsystems, Inc
 *
 * This file is part of PowerTOP
 *
 * This program file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named COPYING; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * Authors:
 *	Arjan van de Ven <arjan@linux.intel.com>
 *	Eric C Saxe <eric.saxe@sun.com>
 *	Aubrey Li <aubrey.li@intel.com>
 */

/*
 * GPL Disclaimer
 *
 * For the avoidance of doubt, except that if any license choice other
 * than GPL or LGPL is available it will apply instead, Sun elects to
 * use only the General Public License version 2 (GPLv2) at this time
 * for any software where a choice of GPL license versions is made
 * available with the language indicating that GPLv2 or any later
 * version may be used, or where a choice of which version of the GPL
 * is applied is otherwise unspecified.
 */

/*
 * DTrace scripts for observing interrupts, callouts and cyclic events
 * that cause CPU activity. Such activity prevents the processor from
 * entering lower power states and reducing power consumption.
 *
 * g_dtp_events is the default script
 */
const char *g_dtp_events =
"interrupt-complete"
"/arg0 != NULL && arg3 !=0/"
"{"
"	this->devi = (struct dev_info *)arg0;"
"	@interrupts[stringof(`devnamesp[this->devi->devi_major].dn_name),"
"	     this->devi->devi_instance] = count();"
"}"
""
"sdt:::callout-start"
"/(caddr_t)((callout_t *)arg0)->c_func == (caddr_t)&`setrun/"
"{"
"       this->thr = (kthread_t *)(((callout_t *)arg0)->c_arg);"
"       @events_u[stringof(this->thr->t_procp->p_user.u_comm)] = count();"
"}"
""
"sdt:::callout-start"
"/(caddr_t)((callout_t *)arg0)->c_func != (caddr_t)&`setrun/"
"{"
"       @events_k[(caddr_t)((callout_t *)arg0)->c_func] = count();"
"}"
""
"sdt:::cyclic-start"
"/(caddr_t)((cyclic_t *)arg0)->cy_handler == (caddr_t)&`clock/"
"{"
"	@events_k[(caddr_t)((cyclic_t *)arg0)->cy_handler] = count();"
"}"
""
"fbt::xt_all:entry,"
"fbt::xc_all:entry"
"{"
"	self->xc_func = arg0;"
"}"
""
"fbt::xt_one_unchecked:entry,"
"fbt::xt_some:entry,"
"fbt::xc_one:entry,"
"fbt::xc_some:entry"
"{"
"	self->xc_func = arg1;"
"}"
""
"sysinfo:::xcalls"
"/pid != $pid/"
"{"
"       @events_x[execname, self->xc_func] = sum(arg0);"
"	self->xc_func = 0;"
"}";

/*
 * g_dtp_events_v is enabled through the -v option, it includes cyclic events
 * in the report, allowing a complete view of system activity
 */
const char *g_dtp_events_v =
"interrupt-complete"
"/arg0 != NULL && arg3 !=0/"
"{"
"	this->devi = (struct dev_info *)arg0;"
"	@interrupts[stringof(`devnamesp[this->devi->devi_major].dn_name),"
"	     this->devi->devi_instance] = count();"
"}"
""
"sdt:::callout-start"
"/(caddr_t)((callout_t *)arg0)->c_func == (caddr_t)&`setrun/"
"{"
"       this->thr = (kthread_t *)(((callout_t *)arg0)->c_arg);"
"       @events_u[stringof(this->thr->t_procp->p_user.u_comm)] = count();"
"}"
""
"sdt:::callout-start"
"/(caddr_t)((callout_t *)arg0)->c_func != (caddr_t)&`setrun/"
"{"
"       @events_k[(caddr_t)((callout_t *)arg0)->c_func] = count();"
"}"
""
"sdt:::cyclic-start"
"/(caddr_t)((cyclic_t *)arg0)->cy_handler != (caddr_t)&`dtrace_state_deadman &&"
" (caddr_t)((cyclic_t *)arg0)->cy_handler != (caddr_t)&`dtrace_state_clean/"
"{"
"	@events_k[(caddr_t)((cyclic_t *)arg0)->cy_handler] = count();"
"}"
""
"fbt::xt_all:entry,"
"fbt::xc_all:entry"
"{"
"	self->xc_func = arg0;"
"}"
""
"fbt::xt_one_unchecked:entry,"
"fbt::xt_some:entry,"
"fbt::xc_one:entry,"
"fbt::xc_some:entry"
"{"
"	self->xc_func = arg1;"
"}"
""
"sysinfo:::xcalls"
"/pid != $pid/"
"{"
"       @events_x[execname, self->xc_func] = sum(arg0);"
"	self->xc_func = 0;"
"}";

/*
 * This script is selected through the -c option, it takes the CPU id as
 * argument and observes activity generated by that CPU
 */
const char *g_dtp_events_c =
"interrupt-complete"
"/cpu == $0 &&"
" arg0 != NULL && arg3 != 0/"
"{"
"	this->devi = (struct dev_info *)arg0;"
"	@interrupts[stringof(`devnamesp[this->devi->devi_major].dn_name),"
"	     this->devi->devi_instance] = count();"
"}"
""
"sdt:::callout-start"
"/cpu == $0 &&"
" (caddr_t)((callout_t *)arg0)->c_func == (caddr_t)&`setrun/"
"{"
"       this->thr = (kthread_t *)(((callout_t *)arg0)->c_arg);"
"       @events_u[stringof(this->thr->t_procp->p_user.u_comm)] = count();"
"}"
""
"sdt:::callout-start"
"/cpu == $0 &&"
" (caddr_t)((callout_t *)arg0)->c_func != (caddr_t)&`setrun/"
"{"
"       @events_k[(caddr_t)((callout_t *)arg0)->c_func] = count();"
"}"
""
"sdt:::cyclic-start"
"/cpu == $0 &&"
" (caddr_t)((cyclic_t *)arg0)->cy_handler == (caddr_t)&`clock/"
"{"
"	@events_k[(caddr_t)((cyclic_t *)arg0)->cy_handler] = count();"
"}"
""
/*
 * xcalls to all CPUs. We're only interested in firings from other CPUs since
 * the system doesn't xcall itself
 */
"fbt::xt_all:entry,"
"fbt::xc_all:entry"
"/pid != $pid &&"
" cpu != $0/"
"{"
"	self->xc_func = arg0;"
"	self->xc_cpu = cpu;"
"	self->cpu_known = 1;"
"}"
""
/*
 * xcalls to a subset of CPUs. No way of knowing if the observed CPU is in
 * it, so account it in the generic @events_x aggregation. Again, we don't
 * xcall the current CPU.
 */
"fbt::xt_some:entry,"
"fbt::xc_some:entry"
"/pid != $pid &&"
" cpu != $0/"
"{"
"	self->xc_func = arg1;"
"}"
""
/*
 * xcalls to a specific CPU, with all the necessary information
 */
"fbt::xt_one_unchecked:entry,"
"fbt::xc_one:entry"
"/arg0 == $0/"
"{"
"	self->xc_func = arg1;"
"	self->xc_cpu = arg0;"
"	self->cpu_known = 1;"
"}"
""
"sysinfo:::xcalls"
"/pid != $pid &&"
" self->xc_func &&"
" !self->cpu_known/"
"{"
"       @events_x[execname, self->xc_func] = sum(arg0);"
"	self->xc_func = 0;"
"}"
""
"sysinfo:::xcalls"
"/pid != $pid &&"
" self->xc_func &&"
" self->cpu_known/"
"{"
"       @events_xc[execname, self->xc_func, self->xc_cpu] = sum(arg0);"
"	self->xc_func = 0;"
"	self->xc_cpu = 0;"
"	self->cpu_known = 0;"
"}";

/*
 * sparcv9 platform specific display messages
 */
const char *g_msg_idle_state = "Idle Power States";
const char *g_msg_freq_state = "Frequency Levels";
const char *g_msg_freq_enable = "P - Enable CPU PM";
