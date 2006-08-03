/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004 Kuwashima
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id: riconv.h 2 2006-04-11 19:04:40Z arton $
 */
#ifndef _RICONV_H
#define _RICONV_H

extern VALUE exticonv_local_to_utf8(VALUE);
extern VALUE exticonv_utf8_to_local(VALUE);
extern VALUE exticonv_cc(VALUE, const char*, const char*);
extern VALUE exticonv_vv(VALUE, VALUE, VALUE);

#endif /* _RICONV_H */
