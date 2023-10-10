/*
 * Copyright (c) 2015 Novell, Inc.
 * Copyright (c) [2017-2019] SUSE LLC
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


#ifndef STORAGE_MOCKUP_H
#define STORAGE_MOCKUP_H


#include <string>
#include <vector>
#include <map>
#include <set>

#include "storage/Utils/Remote.h"


/**
 * With Occam's Razor an exception is thrown when not all entries in
 * the mockup are used and Mockup::occams_razor() is called (which is
 * done after probing when reading the mockup). Also during loading
 * the mockup an exception is thrown when big identical commands or
 * files are found.
 *
 * Enable only for development code but not for production code.
 */
// #define OCCAMS_RAZOR


namespace storage
{
    using std::string;
    using std::vector;
    using std::map;
    using std::set;


    class Mockup
    {
    public:

	typedef RemoteCommand Command;
	typedef RemoteFile File;

	enum class Mode
	{
	    NONE, PLAYBACK, RECORD
	};

	static Mode get_mode() { return mode; }
	static void set_mode(Mode mode) { Mockup::mode = mode; }

	static void load(const string& filename);
	static void save(const string& filename);

	static bool has_command(const string& name);
	static const Command& get_command(const string& name);
	static void set_command(const string& name, const Command& command);
	static void set_command(const vector<string>& name, const Command& command);
	static void erase_command(const string& name);

	static bool has_file(const string& name);
	static const File& get_file(const string& name);
	static void set_file(const string& name, const File& file);
	static void erase_file(const string& name);

	static void occams_razor();

    private:

	static Mode mode;

	static map<string, Command> commands;
	static map<string, File> files;

#ifdef OCCAMS_RAZOR
	const static size_t threshold = 4;

	static set<string> used_commands;
	static set<string> used_files;
#endif

    };

}


#endif
