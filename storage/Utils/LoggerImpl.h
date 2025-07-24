/*
 * Copyright (c) [2014-2015] Novell, Inc.
 * Copyright (c) 2016 SUSE LLC
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


#ifndef STORAGE_LOGGER_IMPL_H
#define STORAGE_LOGGER_IMPL_H


#include <sstream>

#include "storage/Utils/Logger.h"


namespace storage
{

    bool query_log_level(LogLevel log_level);

    std::ostringstream* open_log_stream();

    void close_log_stream(LogLevel log_level, const char* file, unsigned int line,
			  const char* func, std::ostringstream*);

#define y2deb(op) y2log_op(storage::LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__, op)
#define y2mil(op) y2log_op(storage::LogLevel::MILESTONE, __FILE__, __LINE__, __FUNCTION__, op)
#define y2war(op) y2log_op(storage::LogLevel::WARNING, __FILE__, __LINE__, __FUNCTION__, op)
#define y2err(op) y2log_op(storage::LogLevel::ERROR, __FILE__, __LINE__, __FUNCTION__, op)

#define y2log_op(log_level, file, line, func, op)				\
    do {									\
	if (storage::query_log_level(log_level))				\
	{									\
	    std::ostringstream* __buf = storage::open_log_stream();		\
	    *__buf << op;							\
	    storage::close_log_stream(log_level, file, line, func, __buf);	\
	}									\
    } while (0)

}


#endif
