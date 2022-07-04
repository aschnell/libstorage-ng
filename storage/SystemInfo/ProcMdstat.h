/*
 * Copyright (c) [2004-2014] Novell, Inc.
 * Copyright (c) [2017-2020] SUSE LLC
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


#ifndef STORAGE_PROC_MDSTAT_H
#define STORAGE_PROC_MDSTAT_H


#include <map>
#include <vector>

#include "storage/Devices/Md.h"


namespace storage
{
    using std::string;
    using std::map;
    using std::vector;


    class ProcMdstat
    {
    public:

	ProcMdstat();


	struct Device
	{
	    Device(const string& name, bool spare, bool faulty, bool journal)
		: name(name), spare(spare), faulty(faulty), journal(journal) {}

	    string name;
	    bool spare;
	    bool faulty;
	    bool journal;

	    bool operator<(const Device& rhs) const { return name < rhs.name; }

	};


	struct Entry
	{
	    Entry() : md_level(MdLevel::UNKNOWN), md_parity(MdParity::DEFAULT), size(0),
		      chunk_size(0), read_only(false), inactive(false), is_container(false),
		      has_container(false) {}

	    MdLevel md_level;
	    MdParity md_parity;

	    string super;

	    unsigned long long size;
	    unsigned long chunk_size;

	    bool read_only;
	    bool inactive;

	    vector<Device> devices;

	    bool is_container;

	    bool has_container;
	    string container_name;
	    string container_member;
	};

	friend std::ostream& operator<<(std::ostream& s, const ProcMdstat& proc_mdstat);
	friend std::ostream& operator<<(std::ostream& s, const Entry& entry);
	friend std::ostream& operator<<(std::ostream& s, const Device& device);

	vector<string> get_entries() const;

	bool has_entry(const string& name) const;

	const Entry& get_entry(const string& name) const;

	typedef map<string, Entry>::const_iterator const_iterator;

	const_iterator begin() const { return data.begin(); }
	const_iterator end() const { return data.end(); }

    private:

	void parse(const vector<string>& lines);

	Entry parse(const string& line1, const string& line2);

	map<string, Entry> data;

    };


    /**
     * Parse (the --export variant of) mdadm --detail
     */
    class MdadmDetail
    {
    public:

	MdadmDetail(const string& device);

	string uuid;
	string devname;
	string metadata;
	MdLevel level;

	/**
	 * Mapping from device name to role (a number or spare). Faulty and journal
	 * devices are also marked as spare by mdadm here (that might be a bug).
	 */
	map<string, string> roles;

	friend std::ostream& operator<<(std::ostream& s, const MdadmDetail& mdadm_detail);

    private:

	void parse(const vector<string>& lines);

	string device;

    };

}


#endif
