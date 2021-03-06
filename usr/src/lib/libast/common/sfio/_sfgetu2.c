/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*          Copyright (c) 1985-2009 AT&T Intellectual Property          *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                    by AT&T Intellectual Property                     *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                  David Korn <dgk@research.att.com>                   *
*                   Phong Vo <kpv@research.att.com>                    *
*                                                                      *
***********************************************************************/
/* OBSOLETE 19961031 -- for shared library compatibility */

#include	"sfhdr.h"

#undef	_sfgetu2

_BEGIN_EXTERNS_
#if _BLD_sfio && defined(__EXPORT__)
#define extern	__EXPORT__
#endif

extern long	_sfgetu2 _ARG_((Sfio_t*, long));

#undef	extern
_END_EXTERNS_

#if __STD_C
long _sfgetu2(reg Sfio_t* f, long v)
#else
long _sfgetu2(f, v)
reg Sfio_t*	f;
long		v;
#endif
{
	if (v < 0)
		return -1;
	sfungetc(f, v);
	return sfgetu(f);
}
