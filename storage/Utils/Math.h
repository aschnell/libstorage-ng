/*
 * Copyright (c) [2016-2017] SUSE LLC
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#ifndef STORAGE_MATH_H
#define STORAGE_MATH_H


namespace storage
{

    bool
    is_power_of_two(unsigned long long i);


    unsigned long long
    next_power_of_two(unsigned long long i);


    inline bool
    is_multiple_of(unsigned long long i, unsigned long long j)
    {
	return i % j == 0;
    }


    /**
     * Round i up to a multiple of m.
     */
    unsigned long long
    round_up(unsigned long long i, unsigned long long m);

}


#endif
