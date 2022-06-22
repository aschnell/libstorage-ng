/*
 * Copyright (c) [2014-2015] Novell, Inc.
 * Copyright (c) [2016-2022] SUSE LLC
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


#include "config.h"
#include "storage/Utils/AppUtil.h"
#include "storage/Utils/Mockup.h"
#include "storage/StorageImpl.h"
#include "storage/Devices/DiskImpl.h"
#include "storage/Devices/DasdImpl.h"
#include "storage/Devices/MultipathImpl.h"
#include "storage/Devices/DmRaidImpl.h"
#include "storage/Devices/MdImpl.h"
#include "storage/Devices/LvmLvImpl.h"
#include "storage/Devices/LuksImpl.h"
#include "storage/Devices/BitlockerV2Impl.h"
#include "storage/Pool.h"
#include "storage/SystemInfo/SystemInfoImpl.h"
#include "storage/Actiongraph.h"
#include "storage/Prober.h"
#include "storage/EnvironmentImpl.h"
#include "storage/Utils/Format.h"
#include "storage/Utils/CallbacksImpl.h"
#include "storage/Utils/StorageTmpl.h"


namespace storage
{

    sid_t Storage::Impl::global_sid = initial_global_sid;


    Storage::Impl::Impl(Storage& storage, const Environment& environment)
	: storage(storage), environment(environment), arch(false),
	  lock(environment.is_read_only(), !environment.get_impl().is_do_lock()),
	  default_mount_by(MountByType::UUID), rootprefix(environment.get_rootprefix()),
	  tmp_dir("libstorage-XXXXXX")
    {
	y2mil("constructed Storage with " << environment);
	y2mil("libstorage-ng version " VERSION);
	y2mil("timestamp " << timestamp());

	Environment::Impl::extra_log();

	create_devicegraph("probed");
	copy_devicegraph("probed", "staging");
	copy_devicegraph("probed", "system");
    }


    Storage::Impl::~Impl()
    {
	// TODO: Make sure logger is destroyed after this object
    }


    void
    Storage::Impl::activate(const ActivateCallbacks* activate_callbacks) const
    {
	ST_CHECK_PTR(activate_callbacks);

	CallbacksGuard callbacks_guard(activate_callbacks);

	/**
	 * Multipath is activated first since multipath can only use disks.
	 *
	 * MD RAIDs are activated before DM RAIDs since using mdadm is
	 * preferred over dmraid (see fate #316007).
	 *
	 * Activating MDs is only needed if the MDs were stopped manually
	 * since they are otherwise activated by the system automatically.
	 * That is also the reason why there is no loop for MD activations:
	 * MDs on any device that appears are activated automatically,
	 * e.g. nested MDs.
	 *
	 * Activation of LVM and LUKS is done in a loop since it the stacking
	 * can be done in any order.
	 */

	y2mil("activate begin");

	y2mil("rootprefix: " << get_rootprefix());

	const ActivateCallbacksV3* activate_callbacks_v3 = dynamic_cast<const ActivateCallbacksV3*>(activate_callbacks);

	Multipath::Impl::activate_multipaths(activate_callbacks);

	Md::Impl::activate_mds(activate_callbacks, tmp_dir);

	DmRaid::Impl::activate_dm_raids(activate_callbacks);

	while (true)
	{
	    bool again = false;

	    if (LvmLv::Impl::activate_lvm_lvs(activate_callbacks))
		again = true;

	    if (Luks::Impl::activate_lukses(activate_callbacks, storage))
		again = true;

	    if (activate_callbacks_v3 && BitlockerV2::Impl::activate_bitlockers(activate_callbacks_v3, storage))
		again = true;

	    if (!again)
		break;
	}

	y2mil("activate end");
    }


    DeactivateStatusV2
    Storage::Impl::deactivate() const
    {
	y2mil("deactivate begin");

	/**
	 * All deactivate functions return true if nothing is left to
	 * deactivate (so either deactivation was successful or there was
	 * nothing to deactivate).
	 *
	 * So loop at most three times until all deactivate functions returned
	 * true.
	 */

	DeactivateStatusV2 ret;
	DeactivateStatusV2::Impl& deactivate_status = ret.get_impl();

	for (int i = 0; i < 3; ++i)
	{
	    if (!deactivate_status.luks)
		deactivate_status.luks = Luks::Impl::deactivate_lukses();

	    if (!deactivate_status.bitlocker)
		deactivate_status.bitlocker = BitlockerV2::Impl::deactivate_bitlockers();

	    if (!deactivate_status.lvm_lv)
		deactivate_status.lvm_lv = LvmLv::Impl::deactivate_lvm_lvs();

	    if (!deactivate_status.md)
		deactivate_status.md = Md::Impl::deactivate_mds();

	    if (deactivate_status.luks && deactivate_status.bitlocker && deactivate_status.lvm_lv &&
		deactivate_status.md)
		break;
	}

	deactivate_status.dm_raid = DmRaid::Impl::deactivate_dm_raids();

	deactivate_status.multipath = Multipath::Impl::deactivate_multipaths();

	y2mil("deactivate end");

	return ret;
    }


    void
    Storage::Impl::probe(const ProbeCallbacks* probe_callbacks)
    {
	y2mil("probe begin");

	y2mil("rootprefix: " << get_rootprefix());

	CallbacksGuard callbacks_guard(probe_callbacks);

	if (exist_devicegraph("probed"))
	    remove_devicegraph("probed");

	if (exist_devicegraph("staging"))
	    remove_devicegraph("staging");

	if (exist_devicegraph("system"))
	    remove_devicegraph("system");

	// The system devicegraph is created and used for probing since it is
	// needed in EnsureMounted.

	Devicegraph* probed = create_devicegraph("system");

	switch (environment.get_probe_mode())
	{
	    case ProbeMode::STANDARD: {
		probe_helper(probe_callbacks, probed);
	    } break;

	    case ProbeMode::STANDARD_WRITE_DEVICEGRAPH: {
		probe_helper(probe_callbacks, probed);
		probed->save(environment.get_devicegraph_filename());
	    } break;

	    case ProbeMode::STANDARD_WRITE_MOCKUP: {
		Mockup::set_mode(Mockup::Mode::RECORD);
		probe_helper(probe_callbacks, probed);
		Mockup::save(environment.get_mockup_filename());
	    } break;

	    case ProbeMode::NONE: {
	    } break;

	    case ProbeMode::READ_DEVICEGRAPH: {
		probed->load(environment.get_devicegraph_filename());
	    } break;

	    case ProbeMode::READ_MOCKUP: {
		Mockup::set_mode(Mockup::Mode::PLAYBACK);
		Mockup::load(environment.get_mockup_filename());
		probe_helper(probe_callbacks, probed);
		Mockup::occams_razor();
	    } break;
	}

	y2mil("probe end");

	y2mil("probed devicegraph begin");
	y2mil(*probed);
	y2mil("probed devicegraph end");

	copy_devicegraph("system", "staging");
	copy_devicegraph("system", "probed");
    }


    void
    Storage::Impl::probe_helper(const ProbeCallbacks* probe_callbacks, Devicegraph* probed)
    {
	SystemInfo::Impl system_info;

	arch = system_info.getArch();

	Prober prober(storage, probe_callbacks, probed, system_info);
    }


    Devicegraph*
    Storage::Impl::get_devicegraph(const string& name)
    {
	if (name == "probed")
	    ST_THROW(Exception(sformat("invalid devicegraph name '%s'", name)));

	devicegraphs_t::iterator it = devicegraphs.find(name);
	if (it == devicegraphs.end())
	    ST_THROW(Exception(sformat("devicegraph '%s' not found", name)));

	return &it->second;
    }


    const Devicegraph*
    Storage::Impl::get_devicegraph(const string& name) const
    {
	devicegraphs_t::const_iterator it = devicegraphs.find(name);
	if (it == devicegraphs.end())
	    ST_THROW(Exception(sformat("devicegraph '%s' not found", name)));

	return &it->second;
    }


    Devicegraph*
    Storage::Impl::get_staging()
    {
	return get_devicegraph("staging");
    }


    const Devicegraph*
    Storage::Impl::get_staging() const
    {
	return get_devicegraph("staging");
    }


    const Devicegraph*
    Storage::Impl::get_probed() const
    {
	return get_devicegraph("probed");
    }


    Devicegraph*
    Storage::Impl::get_system()
    {
	return get_devicegraph("system");
    }


    const Devicegraph*
    Storage::Impl::get_system() const
    {
	return get_devicegraph("system");
    }


    vector<string>
    Storage::Impl::get_devicegraph_names() const
    {
	vector<string> ret;

	for (const devicegraphs_t::value_type& tmp : devicegraphs)
	    ret.push_back(tmp.first);

	return ret;
    }


    map<string, const Devicegraph*>
    Storage::Impl::get_devicegraphs() const
    {
	map<string, const Devicegraph*> ret;

	for (const devicegraphs_t::value_type& tmp : devicegraphs)
	    ret[tmp.first] = &tmp.second;

	return ret;
    }


    void
    Storage::Impl::verify_devicegraph_name(const string& name) const
    {
	if (name.empty())
	    ST_THROW(Exception(sformat("invalid devicegraph name '%s'", name)));
    }


    Devicegraph*
    Storage::Impl::create_devicegraph(const string& name)
    {
	verify_devicegraph_name(name);

	pair<devicegraphs_t::iterator, bool> tmp =
	    devicegraphs.emplace(piecewise_construct, forward_as_tuple(name),
				 forward_as_tuple(&storage));
	if (!tmp.second)
	    ST_THROW(Exception(sformat("devicegraph '%s' already exists", name)));

	devicegraphs_t::iterator it = tmp.first;

	return &it->second;
    }


    Devicegraph*
    Storage::Impl::copy_devicegraph(const string& source_name, const string& dest_name)
    {
	const Devicegraph* tmp1 = static_cast<const Impl*>(this)->get_devicegraph(source_name);

	Devicegraph* tmp2 = create_devicegraph(dest_name);

	tmp1->copy(*tmp2);

	return tmp2;
    }


    void
    Storage::Impl::remove_devicegraph(const string& name)
    {
	if (devicegraphs.erase(name) == 0)
	    ST_THROW(Exception(sformat("devicegraph '%s' not found", name)));
    }


    void
    Storage::Impl::restore_devicegraph(const string& name)
    {
	devicegraphs_t::iterator it1 = devicegraphs.find(name);
	if (it1 == devicegraphs.end())
	    ST_THROW(Exception(sformat("devicegraph '%s' not found", name)));

	devicegraphs_t::iterator it2 = devicegraphs.find("staging");
	if (it2 == devicegraphs.end())
	    ST_THROW(Exception(sformat("devicegraph '%s' not found", name)));

	devicegraphs.erase(it2);

	map<string, Devicegraph>::node_type node = devicegraphs.extract(it1);
	node.key() = "staging";
	devicegraphs.insert(std::move(node));
    }


    bool
    Storage::Impl::exist_devicegraph(const string& name) const
    {
	return devicegraphs.find(name) != devicegraphs.end();
    }


    bool
    Storage::Impl::equal_devicegraph(const string& lhs, const string& rhs) const
    {
	return *get_devicegraph(lhs) == *get_devicegraph(rhs);
    }


    void
    Storage::Impl::check(const CheckCallbacks* check_callbacks) const
    {
	// check all devicegraphs

	// check that all objects with the same sid have the same type in all
	// devicegraphs

	map<sid_t, set<string>> all_sids_with_types;

	for (const devicegraphs_t::value_type& key_value : devicegraphs)
	{
	    const Devicegraph& devicegraph = key_value.second;

	    devicegraph.check(check_callbacks);

	    for (Devicegraph::Impl::vertex_descriptor vertex : devicegraph.get_impl().vertices())
	    {
		const Device* device = devicegraph.get_impl()[vertex];
		all_sids_with_types[device->get_sid()].insert(device->get_impl().get_classname());
	    }
	}

	for (const map<sid_t, set<string>>::value_type& key_value : all_sids_with_types)
	{
	    if (key_value.second.size() != 1)
	    {
		stringstream tmp;
		tmp << key_value.second;

		ST_THROW(Exception(sformat("objects with sid %d have different types %s",
					   key_value.first, tmp.str())));
	    }
	}
    }


    string
    Storage::Impl::prepend_rootprefix(const string& mount_point) const
    {
	if (mount_point == "swap" || rootprefix.empty())
	    return mount_point;

	if (mount_point == "/")
	    return rootprefix;
	else
	    return rootprefix + mount_point;
    }


    const Actiongraph*
    Storage::Impl::calculate_actiongraph()
    {
	actiongraph = nullptr;	// free old actiongraph before generating new to avoid memory peak

	unique_ptr<Actiongraph> tmp = make_unique<Actiongraph>(storage, get_system(), get_staging());
	tmp->generate_compound_actions();

	actiongraph = std::move(tmp);

	return actiongraph.get();
    }


    void
    Storage::Impl::commit(const CommitOptions& commit_options, const CommitCallbacks* commit_callbacks)
    {
	ST_CHECK_PTR(actiongraph.get());

	actiongraph->get_impl().commit(commit_options, commit_callbacks);

	// TODO somehow update probed
    }


    void
    Storage::Impl::generate_pools(const Devicegraph* devicegraph)
    {
	ST_CHECK_PTR(devicegraph);

	// TODO check partition table?

	for (const Partitionable* partitionable : Partitionable::get_all(devicegraph))
	{
	    // Ignore partitionables with size zero - maybe some card reader.
	    if (partitionable->get_size() == 0)
		continue;

	    // Ignore partitionables used by multipath or raid.
	    vector<const Device*> children = partitionable->get_children();
	    if (any_of(children.begin(), children.end(), [](const Device* child) {
		return is_multipath(child) || is_dm_raid(child) || is_md(child);
	    }))
		continue;

	    string name = partitionable->get_impl().pool_name();
	    if (name.empty())
		continue;

	    name += " (" + byte_to_humanstring(partitionable->get_region().get_block_size(), false, 2, true) + ")";

	    Pool* pool = exists_pool(name) ? get_pool(name) : create_pool(name);

	    if (!pool->exists_device(partitionable))
		pool->add_device(partitionable);
	}
    }


    void
    Storage::Impl::verify_pool_name(const string& name) const
    {
	if (name.empty())
	    ST_THROW(Exception(sformat("invalid pool name '%s'", name)));
    }


    Pool*
    Storage::Impl::create_pool(const std::string& name)
    {
	verify_pool_name(name);

	pair<pools_t::iterator, bool> tmp =
	    pools.emplace(piecewise_construct, forward_as_tuple(name),
			  forward_as_tuple());
	if (!tmp.second)
	    ST_THROW(Exception(sformat("pool '%s' already exists", name)));

	pools_t::iterator it = tmp.first;

	return &it->second;
    }


    void
    Storage::Impl::remove_pool(const string& name)
    {
	if (pools.erase(name) == 0)
	    ST_THROW(Exception(sformat("pool '%s' not found", name)));
    }


    void
    Storage::Impl::rename_pool(const string& old_name, const string& new_name)
    {
	pools_t::iterator it1 = pools.find(old_name);
	if (it1 == pools.end())
	    ST_THROW(Exception(sformat("pool '%s' not found", old_name)));

	pools_t::iterator it2 = pools.find(new_name);
	if (it2 != pools.end())
	    ST_THROW(Exception(sformat("pool '%s' already exists", new_name)));

	map<string, Pool>::node_type node = pools.extract(it1);
	node.key() = new_name;
	pools.insert(std::move(node));
    }


    bool
    Storage::Impl::exists_pool(const string& name) const
    {
	return pools.find(name) != pools.end();
    }


    vector<string>
    Storage::Impl::get_pool_names() const
    {
	vector<string> ret;

	for (const pools_t::value_type& tmp : pools)
	    ret.push_back(tmp.first);

	return ret;
    }


    map<string, Pool*>
    Storage::Impl::get_pools()
    {
	map<string, Pool*> ret;

	for (pools_t::value_type& tmp : pools)
	    ret[tmp.first] = &tmp.second;

	return ret;
    }


    map<string, const Pool*>
    Storage::Impl::get_pools() const
    {
	map<string, const Pool*> ret;

	for (const pools_t::value_type& tmp : pools)
	    ret[tmp.first] = &tmp.second;

	return ret;
    }


    Pool*
    Storage::Impl::get_pool(const string& name)
    {
	pools_t::iterator it = pools.find(name);
	if (it == pools.end())
	    ST_THROW(Exception(sformat("pool '%s' not found", name)));

	return &it->second;
    }


    const Pool*
    Storage::Impl::get_pool(const string& name) const
    {
	pools_t::const_iterator it = pools.find(name);
	if (it == pools.end())
	    ST_THROW(Exception(sformat("pool '%s' not found", name)));

	return &it->second;
    }

}
