/*
 * Copyright (c) 2015 Novell, Inc.
 * Copyright (c) [2019-2023] SUSE LLC
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


#ifndef STORAGE_REMOTE_H
#define STORAGE_REMOTE_H


#include <string>
#include <vector>


// TODO the classnames are bad since the classes for also used in Mockup
// TODO swig does not support nested classes so here are currently ugly names

namespace storage
{

    /**
     * A result of an unnamed command: stdout + stderr + exit_code.
     */
    struct RemoteCommand
    {
	RemoteCommand() = default;
	RemoteCommand(const std::vector<std::string>& stdout)
	    : stdout(stdout) {}
	RemoteCommand(const std::vector<std::string>& stdout,
		      const std::vector<std::string>& stderr, int exit_code)
	    : stdout(stdout), stderr(stderr), exit_code(exit_code) {}

	std::vector<std::string> stdout;
	std::vector<std::string> stderr;
	int exit_code = 0;

	/**
	 * Compare stdout, stderr and exit_code of two RemoteCommands
	 */
	bool operator==(const RemoteCommand& rhs) const;
    };


    /**
     * Contents of an unnamed file (vector of lines)
     */
    struct RemoteFile
    {
	RemoteFile() = default;
	RemoteFile(const std::vector<std::string>& content) : content(content) {}

	std::vector<std::string> content;

	/**
	 * Compare content of two Remotefiles.
	 */
	bool operator==(const RemoteFile& rhs) const;
    };


    class RemoteCallbacks
    {
    public:

	virtual ~RemoteCallbacks() {}

	virtual RemoteCommand get_command(const std::string& name) const = 0;
	virtual RemoteFile get_file(const std::string& name) const = 0;

    };


    class RemoteCallbacksV2 : public RemoteCallbacks
    {
    public:

	virtual ~RemoteCallbacksV2() {}

	virtual RemoteCommand get_command_v2(const std::vector<std::string>& args) const = 0;

    };

    const RemoteCallbacks* get_remote_callbacks();
    void set_remote_callbacks(const RemoteCallbacks* remote_callbacks);

}


#endif
