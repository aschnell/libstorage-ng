/*
 * Copyright (c) 2015 Novell, Inc.
 * Copyright (c) [2016-2021] SUSE LLC
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


#include "storage/Devices/BlkDeviceImpl.h"
#include "storage/Filesystems/BtrfsImpl.h"
#include "storage/Filesystems/BtrfsSubvolumeImpl.h"
#include "storage/Filesystems/BtrfsQgroupImpl.h"
#include "storage/Filesystems/MountPointImpl.h"
#include "storage/DevicegraphImpl.h"
#include "storage/Utils/StorageDefines.h"
#include "storage/Utils/HumanString.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/XmlFile.h"
#include "storage/Utils/StorageTmpl.h"
#include "storage/Utils/Format.h"
#include "storage/Utils/CallbacksImpl.h"
#include "storage/FreeInfo.h"
#include "storage/UsedFeatures.h"
#include "storage/SystemInfo/SystemInfoImpl.h"
#include "storage/Holders/Subdevice.h"
#include "storage/Holders/FilesystemUserImpl.h"
#include "storage/Holders/Snapshot.h"
#include "storage/Holders/BtrfsQgroupRelationImpl.h"
#include "storage/EnvironmentImpl.h"
#include "storage/Storage.h"
#include "storage/Utils/Mockup.h"
#include "storage/Prober.h"
#include "storage/Redirect.h"
#include "storage/Actions/Reallot.h"
#include "storage/Actions/SetLabel.h"
#include "storage/Actions/SetQuota.h"
#include "storage/Actions/Create.h"


namespace storage
{

    using namespace std;


    const char* DeviceTraits<Btrfs>::classname = "Btrfs";


    const vector<string> EnumTraits<BtrfsRaidLevel>::names({
	"UNKNOWN", "DEFAULT", "SINGLE", "DUP", "RAID0", "RAID1", "RAID5", "RAID6", "RAID10",
	"RAID1C3", "RAID1C4"
    });


    Btrfs::Impl::Impl()
	: BlkFilesystem::Impl(), metadata_raid_level(BtrfsRaidLevel::DEFAULT),
	  data_raid_level(BtrfsRaidLevel::DEFAULT)
    {
    }


    Btrfs::Impl::Impl(const xmlNode* node)
	: BlkFilesystem::Impl(node)
    {
	string tmp;

	if (getChildValue(node, "metadata-raid-level", tmp))
	    metadata_raid_level = toValueWithFallback(tmp, BtrfsRaidLevel::UNKNOWN);

	if (getChildValue(node, "data-raid-level", tmp))
	    data_raid_level = toValueWithFallback(tmp, BtrfsRaidLevel::UNKNOWN);

	getChildValue(node, "quota", quota);
    }


    Btrfs::Impl::~Impl()
    {
	delete snapper_config;
    }


    void
    Btrfs::Impl::save(xmlNode* node) const
    {
	BlkFilesystem::Impl::save(node);

	setChildValue(node, "metadata-raid-level", toString(metadata_raid_level));
	setChildValue(node, "data-raid-level", toString(data_raid_level));

	setChildValueIf(node, "quota", quota, quota);
    }


    string
    Btrfs::Impl::get_pretty_classname() const
    {
	// TRANSLATORS: name of object
	return _("Btrfs").translated;
    }


    void
    Btrfs::Impl::check(const CheckCallbacks* check_callbacks) const
    {
	BlkFilesystem::Impl::check(check_callbacks);

	if (num_children_of_type<const BtrfsSubvolume>() != 1)
	    ST_THROW(Exception("top-level subvolume missing"));
    }


    vector<BtrfsRaidLevel>
    Btrfs::Impl::get_allowed_metadata_raid_levels() const
    {
	vector<const BlkDevice*> devices = get_blk_devices();

	// For some number of devices more RAID levels work, e.g. RAID5 with two devices,
	// but are not recommended (warning in mkfs.btrfs output) and are also not
	// mentioned in the btrfs wiki
	// (https://btrfs.wiki.kernel.org/index.php/Using_Btrfs_with_Multiple_Devices).

	switch (devices.size())
	{
	    case 0:
		return { };

	    case 1:
		return { BtrfsRaidLevel::SINGLE, BtrfsRaidLevel::DUP };

	    case 2:
		return { BtrfsRaidLevel::SINGLE, BtrfsRaidLevel::RAID0, BtrfsRaidLevel::RAID1 };

	    case 3:
		return { BtrfsRaidLevel::SINGLE, BtrfsRaidLevel::RAID0, BtrfsRaidLevel::RAID1,
			 BtrfsRaidLevel::RAID1C3, BtrfsRaidLevel::RAID5 };

	    default:
		return { BtrfsRaidLevel::SINGLE, BtrfsRaidLevel::RAID0, BtrfsRaidLevel::RAID1,
			 BtrfsRaidLevel::RAID1C3, BtrfsRaidLevel::RAID1C4, BtrfsRaidLevel::RAID5,
			 BtrfsRaidLevel::RAID6, BtrfsRaidLevel::RAID10 };
	}
    }


    vector<BtrfsRaidLevel>
    Btrfs::Impl::get_allowed_data_raid_levels() const
    {
	return get_allowed_metadata_raid_levels();
    }


    void
    Btrfs::Impl::set_quota(bool quota)
    {
	if (Impl::quota == quota)
	    return;

	Impl::quota = quota;

	Devicegraph* devicegraph = get_devicegraph();

	if (quota)
	{
	    bool restore = false;

	    const Devicegraph* probed = get_storage()->get_probed();;
	    const Btrfs* btrfs_probed = nullptr;

	    if (exists_in_probed())
	    {
		btrfs_probed = redirect_to(probed, get_non_impl());
		restore = btrfs_probed->has_quota();
	    }

	    if (restore)
	    {
		// Copy all qgroups from probed.

		for (const BtrfsQgroup* btrfs_qgroup : btrfs_probed->get_btrfs_qgroups())
		{
		    btrfs_qgroup->copy_to_devicegraph(devicegraph);
		}

		// Copy all qgroup relations from probed unless the corresponding
		// subvolume was removed meanwhile.

		for (const BtrfsQgroup* btrfs_qgroup : btrfs_probed->get_btrfs_qgroups())
		{
		    vector<const BtrfsQgroupRelation*> btrfs_qgroup_relations =
			btrfs_qgroup->get_impl().get_in_holders_of_type<const BtrfsQgroupRelation>(View::ALL);
		    for (const BtrfsQgroupRelation* btrfs_qgroup_relation : btrfs_qgroup_relations)
		    {
			if (btrfs_qgroup_relation->get_source()->exists_in_staging())
			    btrfs_qgroup_relation->copy_to_devicegraph(devicegraph);
		    }
		}

		// Create implicit qgroups for new subvolumes.

		for (BtrfsSubvolume* subvolume : get_btrfs_subvolumes())
		{
		    if (!subvolume->exists_in_probed())
			subvolume->create_btrfs_qgroup();
		}
	    }
	    else
	    {
		// Create implicit qgroups for all subvolumes.

		for (BtrfsSubvolume* subvolume : get_btrfs_subvolumes())
		{
		    subvolume->create_btrfs_qgroup();
		}
	    }
	}
	else
	{
	    // When turing off quota remove all qgroups.

	    for (BtrfsQgroup* btrfs_qgroup : get_btrfs_qgroups())
		devicegraph->remove_device(btrfs_qgroup);
	}
    }


    FilesystemUser*
    Btrfs::Impl::add_device(BlkDevice* blk_device)
    {
	ST_CHECK_PTR(blk_device);

	if (blk_device->num_children() != 0)
	    ST_THROW(WrongNumberOfChildren(blk_device->num_children(), 0));

	FilesystemUser* filesystem_user = FilesystemUser::create(get_devicegraph(), blk_device, get_non_impl());

	return filesystem_user;
    }


    void
    Btrfs::Impl::remove_device(BlkDevice* blk_device)
    {
	ST_CHECK_PTR(blk_device);

	FilesystemUser* filesystem_user = to_filesystem_user(get_devicegraph()->find_holder(blk_device->get_sid(),
											    get_sid()));

	get_devicegraph()->remove_holder(filesystem_user);
    }


    BtrfsSubvolume*
    Btrfs::Impl::get_top_level_btrfs_subvolume()
    {
	vector<BtrfsSubvolume*> tmp = get_children_of_type<BtrfsSubvolume>();
	if (tmp.size() != 1)
	    ST_THROW(Exception("no top-level subvolume found"));

	return tmp.front();
    }


    const BtrfsSubvolume*
    Btrfs::Impl::get_top_level_btrfs_subvolume() const
    {
	vector<const BtrfsSubvolume*> tmp = get_children_of_type<const BtrfsSubvolume>();
	if (tmp.size() != 1)
	    ST_THROW(Exception("no top-level subvolume found"));

	return tmp.front();
    }


    BtrfsSubvolume*
    Btrfs::Impl::get_default_btrfs_subvolume()
    {
	for (BtrfsSubvolume* btrfs_subvolume : get_btrfs_subvolumes())
	{
	    if (btrfs_subvolume->is_default_btrfs_subvolume())
		return btrfs_subvolume;
	}

	ST_THROW(Exception("no default btrfs subvolume found"));
    }


    const BtrfsSubvolume*
    Btrfs::Impl::get_default_btrfs_subvolume() const
    {
	for (const BtrfsSubvolume* btrfs_subvolume : get_btrfs_subvolumes())
	{
	    if (btrfs_subvolume->is_default_btrfs_subvolume())
		return btrfs_subvolume;
	}

	ST_THROW(Exception("no default btrfs subvolume found"));
    }


    void
    Btrfs::Impl::set_default_btrfs_subvolume(BtrfsSubvolume* btrfs_subvolume) const
    {
	btrfs_subvolume->set_default_btrfs_subvolume();
    }


    vector<BtrfsSubvolume*>
    Btrfs::Impl::get_btrfs_subvolumes()
    {
	Devicegraph::Impl& devicegraph = get_devicegraph()->get_impl();

	return devicegraph.filter_devices_of_type<BtrfsSubvolume>(
	    devicegraph.descendants(get_vertex(), false)
	);
    }


    vector<const BtrfsSubvolume*>
    Btrfs::Impl::get_btrfs_subvolumes() const
    {
	const Devicegraph::Impl& devicegraph = get_devicegraph()->get_impl();

	return devicegraph.filter_devices_of_type<const BtrfsSubvolume>(
	    devicegraph.descendants(get_vertex(), false)
	);
    }


    bool
    Btrfs::Impl::predicate_etc_fstab(const FstabEntry* fstab_entry) const
    {
	return !fstab_entry->get_mount_opts().has_subvol();
    }


    bool
    Btrfs::Impl::predicate_proc_mounts(const FstabEntry* fstab_entry) const
    {
	long default_id = get_default_btrfs_subvolume()->get_id();

	return !fstab_entry->get_mount_opts().has_subvol() || fstab_entry->get_mount_opts().has_subvol(default_id);
    }


    const BlkDevice*
    Btrfs::Impl::get_etc_fstab_blk_device(const MountPoint* mount_point) const
    {
	const FstabAnchor& fstab_anchor = mount_point->get_impl().get_fstab_anchor();

	if (fstab_anchor.id != 0)
	{
	    for (const Holder* holder : get_non_impl()->get_in_holders())
	    {
		const FilesystemUser* filesystem_user = to_filesystem_user(holder);

		if (filesystem_user->get_impl().get_id() == fstab_anchor.id)
		    return to_blk_device(filesystem_user->get_source());
	    }
	}

	return get_blk_device();
    }


    BtrfsSubvolume*
    Btrfs::Impl::find_btrfs_subvolume_by_path(const string& path)
    {
	for (BtrfsSubvolume* btrfs_subvolume : get_btrfs_subvolumes())
	{
	    if (btrfs_subvolume->get_path() == path)
		return btrfs_subvolume;
	}

	ST_THROW(BtrfsSubvolumeNotFoundByPath(path));
    }


    const BtrfsSubvolume*
    Btrfs::Impl::find_btrfs_subvolume_by_path(const string& path) const
    {
	for (const BtrfsSubvolume* btrfs_subvolume : get_btrfs_subvolumes())
	{
	    if (btrfs_subvolume->get_path() == path)
		return btrfs_subvolume;
	}

	ST_THROW(BtrfsSubvolumeNotFoundByPath(path));
    }


    BtrfsQgroup*
    Btrfs::Impl::create_btrfs_qgroup(const BtrfsQgroup::id_t& id)
    {
	if (!has_quota())
	    ST_THROW(Exception("quota disabled"));

	if (id != BtrfsQgroup::Impl::unknown_id)
	{
	    for (const BtrfsQgroup* btrfs_qgroup : get_btrfs_qgroups())
	    {
		if (btrfs_qgroup->get_id() == id)
		    ST_THROW(Exception("qgroup id already exists"));
	    }
	}

	Devicegraph* devicegraph = get_devicegraph();

	BtrfsQgroup* btrfs_qgroup = BtrfsQgroup::create(devicegraph, id);
	BtrfsQgroupRelation::create(devicegraph, get_non_impl(), btrfs_qgroup);

	return btrfs_qgroup;
    }


    vector<BtrfsQgroup*>
    Btrfs::Impl::get_btrfs_qgroups()
    {
	Devicegraph::Impl& devicegraph = get_devicegraph()->get_impl();

	return devicegraph.filter_devices_of_type<BtrfsQgroup>(
	    devicegraph.descendants(get_vertex(), false, View::ALL)
	);
    }


    vector<const BtrfsQgroup*>
    Btrfs::Impl::get_btrfs_qgroups() const
    {
	const Devicegraph::Impl& devicegraph = get_devicegraph()->get_impl();

	return devicegraph.filter_devices_of_type<const BtrfsQgroup>(
	    devicegraph.descendants(get_vertex(), false, View::ALL)
	);
    }


    BtrfsQgroup*
    Btrfs::Impl::find_btrfs_qgroup_by_id(const BtrfsQgroup::id_t& id)
    {
	for (BtrfsQgroup* btrfs_qgroup : get_btrfs_qgroups())
	{
	    if (btrfs_qgroup->get_id() == id)
		return btrfs_qgroup;
	}

	ST_THROW(BtrfsQgroupNotFoundById(id));
    }


    const BtrfsQgroup*
    Btrfs::Impl::find_btrfs_qgroup_by_id(const BtrfsQgroup::id_t& id) const
    {
	if (!has_quota())
	    ST_THROW(Exception("quota disabled"));

	for (const BtrfsQgroup* btrfs_qgroup : get_btrfs_qgroups())
	{
	    if (btrfs_qgroup->get_id() == id)
		return btrfs_qgroup;
	}

	ST_THROW(BtrfsQgroupNotFoundById(id));
    }


    bool
    Btrfs::Impl::equal(const Device::Impl& rhs_base) const
    {
	const Impl& rhs = dynamic_cast<const Impl&>(rhs_base);

	if (!BlkFilesystem::Impl::equal(rhs))
	    return false;

	return metadata_raid_level == rhs.metadata_raid_level && data_raid_level == rhs.data_raid_level &&
	    quota == rhs.quota;
    }


    void
    Btrfs::Impl::log_diff(std::ostream& log, const Device::Impl& rhs_base) const
    {
	const Impl& rhs = dynamic_cast<const Impl&>(rhs_base);

	BlkFilesystem::Impl::log_diff(log, rhs);

	storage::log_diff_enum(log, "metadata-raid-level", metadata_raid_level, rhs.metadata_raid_level);
	storage::log_diff_enum(log, "data-raid-level", data_raid_level, rhs.data_raid_level);

	storage::log_diff(log, "quota", quota, rhs.quota);
    }


    void
    Btrfs::Impl::print(std::ostream& out) const
    {
	BlkFilesystem::Impl::print(out);

	out << " metadata-raid-level:" << toString(metadata_raid_level) << " data-raid-level:"
	    << toString(data_raid_level);

	if (quota)
	    out << " quota";
    }


    void
    Btrfs::Impl::add_create_actions(Actiongraph::Impl& actiongraph) const
    {
	vector<shared_ptr<Action::Base>> actions;

	actions.push_back(make_shared<Action::Create>(get_sid()));

	if (!get_label().empty())
	    actions.push_back(make_shared<Action::SetLabel>(get_sid()));

	if (quota)
	    actions.push_back(make_shared<Action::SetQuota>(get_sid()));

	actiongraph.add_chain(actions);
    }


    void
    Btrfs::Impl::add_modify_actions(Actiongraph::Impl& actiongraph, const Device* lhs_base) const
    {
	BlkFilesystem::Impl::add_modify_actions(actiongraph, lhs_base);

	const Impl& lhs = dynamic_cast<const Impl&>(lhs_base->get_impl());

	if (quota != lhs.quota)
	{
	    shared_ptr<Action::Base> action = make_shared<Action::SetQuota>(get_sid());
	    actiongraph.add_vertex(action);
	}
    }


    void
    Btrfs::Impl::probe_btrfses(Prober& prober)
    {
	if (!support_btrfs_multiple_devices())
	    return;

	SystemInfo::Impl& system_info = prober.get_system_info();
	Devicegraph* system = prober.get_system();

	const CmdBtrfsFilesystemShow& cmd_btrfs_filesystem_show = system_info.getCmdBtrfsFilesystemShow();

	for (const CmdBtrfsFilesystemShow::value_type& detected_btrfs : cmd_btrfs_filesystem_show)
	{
	    try
	    {
		if (detected_btrfs.devices.empty())
		    ST_THROW(Exception("btrfs has no blk devices"));

		// generate list of blk_device and corresponding id (btrfs devid)
		vector<pair<BlkDevice*, unsigned int>> blk_devices;

		for (const CmdBtrfsFilesystemShow::Device& device : detected_btrfs.devices)
		    blk_devices.emplace_back(BlkDevice::Impl::find_by_any_name(system, device.name, system_info),
					     device.id);

		BlkFilesystem* blk_filesystem = nullptr;

		for (const pair<BlkDevice*, unsigned int>& blk_device : blk_devices)
		{
		    FilesystemUser* filesystem_user = nullptr;

		    if (!blk_filesystem)
		    {
			blk_filesystem = blk_device.first->create_blk_filesystem(FsType::BTRFS);
			filesystem_user = to_filesystem_user(blk_filesystem->get_in_holders().front());
		    }
		    else
		    {
			filesystem_user = FilesystemUser::create(system, blk_device.first, blk_filesystem);
		    }

		    filesystem_user->get_impl().set_id(blk_device.second);
		}

		if (!blk_filesystem)
		    ST_THROW(Exception("no btrfs created"));

		blk_filesystem->get_impl().probe_pass_2a(prober);
		blk_filesystem->get_impl().probe_pass_2b(prober);
	    }
	    catch (const Exception& exception)
	    {
		// TRANSLATORS: error message
		prober.handle(exception, sformat(_("Probing file system with UUID %s failed"),
						 detected_btrfs.uuid), UF_BTRFS);
	    }
	}
    }


    void
    Btrfs::Impl::probe_pass_2a(Prober& prober)
    {
	BlkFilesystem::Impl::probe_pass_2a(prober);

	SystemInfo::Impl& system_info = prober.get_system_info();

	const BlkDevice* blk_device = get_blk_device();

	map<long, BtrfsSubvolume*> subvolumes_by_id;
	map<string, BtrfsSubvolume*> subvolumes_by_uuid;

	BtrfsSubvolume* top_level = get_top_level_btrfs_subvolume();
	subvolumes_by_id[top_level->get_id()] = top_level;

	unique_ptr<EnsureMounted> ensure_mounted;
	string mount_point = "/tmp/does-not-matter";
	if (Mockup::get_mode() != Mockup::Mode::PLAYBACK)
	{
	    ensure_mounted = make_unique<EnsureMounted>(top_level);
	    mount_point = ensure_mounted->get_any_mount_point();
	}

	// Unfortunately 'btrfs subvolume list' uses the UUID to show the parent/origin of
	// snapshots instead of the ID. Also unfortunately the top-level subvolume is not
	// included in the output so the UUID of the top-level must be obtained
	// separately. All hope rests on the JSON output to be better suited (maybe even
	// include the COW flag).

	const CmdBtrfsSubvolumeShow& cmd_btrfs_subvolume_show =
	    system_info.getCmdBtrfsSubvolumeShow(blk_device->get_name(), mount_point);

	if (!cmd_btrfs_subvolume_show.get_uuid().empty())
	    subvolumes_by_uuid[cmd_btrfs_subvolume_show.get_uuid()] = top_level;

	const CmdBtrfsSubvolumeList& cmd_btrfs_subvolume_list =
	    system_info.getCmdBtrfsSubvolumeList(blk_device->get_name(), mount_point);

	// Children can be listed after parents in output of 'btrfs subvolume
	// list ...' so several passes over the list of subvolumes are needed.

	for (const CmdBtrfsSubvolumeList::Entry& subvolume : cmd_btrfs_subvolume_list)
	{
	    BtrfsSubvolume* btrfs_subvolume = BtrfsSubvolume::create(prober.get_system(), subvolume.path);
	    btrfs_subvolume->get_impl().set_id(subvolume.id);

	    subvolumes_by_id[subvolume.id] = btrfs_subvolume;
	    subvolumes_by_uuid[subvolume.uuid] = btrfs_subvolume;
	}

	for (const CmdBtrfsSubvolumeList::Entry& subvolume : cmd_btrfs_subvolume_list)
	{
	    const BtrfsSubvolume* parent = subvolumes_by_id[subvolume.parent_id];
	    if (!parent)
		ST_THROW(Exception("parent subvolume not found by id"));

	    const BtrfsSubvolume* child = subvolumes_by_id[subvolume.id];
	    if (!child)
		ST_THROW(Exception("child subvolume not found by id"));

	    Subdevice::create(prober.get_system(), parent, child);
	}

	for (const CmdBtrfsSubvolumeList::Entry& subvolume : cmd_btrfs_subvolume_list)
	{
	    BtrfsSubvolume* btrfs_subvolume = subvolumes_by_id[subvolume.id];
	    if (!btrfs_subvolume)
		ST_THROW(Exception("subvolume not found by id"));

	    btrfs_subvolume->get_impl().probe_pass_2a(prober, mount_point);
	}

	if (subvolumes_by_id.size() > 1)
	{
	    const CmdBtrfsSubvolumeGetDefault& cmd_btrfs_subvolume_get_default =
		system_info.getCmdBtrfsSubvolumeGetDefault(blk_device->get_name(), mount_point);

	    BtrfsSubvolume* btrfs_subvolume = subvolumes_by_id[cmd_btrfs_subvolume_get_default.get_id()];
	    if (!btrfs_subvolume)
		ST_THROW(Exception("subvolume not found by id"));

	    btrfs_subvolume->get_impl().set_default_btrfs_subvolume();
	}

	for (const CmdBtrfsSubvolumeList::Entry& subvolume : cmd_btrfs_subvolume_list)
	{
	    if (subvolume.parent_uuid.empty())
		continue;

	    if (support_btrfs_snapshot_relations())
	    {
		// The parent UUID is nothing more than a hint (bsc#1179061).

		const BtrfsSubvolume* parent = subvolumes_by_uuid[subvolume.parent_uuid];
		if (!parent)
		    continue;

		const BtrfsSubvolume* child = subvolumes_by_uuid[subvolume.uuid];
		if (!child)
		    ST_THROW(Exception("child subvolume not found by uuid"));

		Snapshot::create(prober.get_system(), parent, child);
	    }
	}

	const CmdBtrfsFilesystemDf& cmd_btrfs_filesystem_df =
	    system_info.getCmdBtrfsFilesystemDf(blk_device->get_name(), mount_point);

	metadata_raid_level = cmd_btrfs_filesystem_df.get_metadata_raid_level();
	data_raid_level = cmd_btrfs_filesystem_df.get_data_raid_level();

	if (support_btrfs_qgroups())
	{
	    const CmdBtrfsQgroupShow& cmd_btrfs_qgroup_show =
		system_info.getCmdBtrfsQgroupShow(blk_device->get_name(), mount_point);

	    quota = cmd_btrfs_qgroup_show.has_quota();

	    map<BtrfsQgroup::id_t, BtrfsQgroup*> btrfs_qgroups_by_id;

	    for (const CmdBtrfsQgroupShow::Entry& qgroup : cmd_btrfs_qgroup_show)
	    {
		BtrfsQgroup* btrfs_qgroup = create_btrfs_qgroup(qgroup.id);

		btrfs_qgroup->get_impl().set_referenced(qgroup.referenced);
		btrfs_qgroup->get_impl().set_exclusive(qgroup.exclusive);
		btrfs_qgroup->get_impl().set_referenced_limit(qgroup.referenced_limit);
		btrfs_qgroup->get_impl().set_exclusive_limit(qgroup.exclusive_limit);

		btrfs_qgroups_by_id[qgroup.id] = btrfs_qgroup;

		if (qgroup.id.first == 0)
		{
		    BtrfsSubvolume* parent = subvolumes_by_id[qgroup.id.second];
		    if (parent)
			BtrfsQgroupRelation::create(prober.get_system(), parent, btrfs_qgroup);
		}
	    }

	    for (const CmdBtrfsQgroupShow::Entry& qgroup : cmd_btrfs_qgroup_show)
	    {
		for (const BtrfsQgroup::id_t& parent_id : qgroup.parents_id)
		{
		    BtrfsQgroup* child = btrfs_qgroups_by_id[parent_id];
		    if (!child)
			ST_THROW(Exception("child qgroup not found by id"));

		    BtrfsQgroup* parent = btrfs_qgroups_by_id[qgroup.id];
		    if (!parent)
			ST_THROW(Exception("parent qgroup not found by id"));

		    BtrfsQgroupRelation::create(prober.get_system(), parent, child);
		}
	    }
	}
    }


    void
    Btrfs::Impl::probe_pass_2b(Prober& prober)
    {
	BlkFilesystem::Impl::probe_pass_2b(prober);

	BtrfsSubvolume* top_level = get_top_level_btrfs_subvolume();

	unique_ptr<EnsureMounted> ensure_mounted;
	string mount_point = "/tmp/does-not-matter";
	if (Mockup::get_mode() != Mockup::Mode::PLAYBACK)
	{
	    ensure_mounted = make_unique<EnsureMounted>(top_level);
	    mount_point = ensure_mounted->get_any_mount_point();
	}

	vector<BtrfsSubvolume*> btrfs_subvolumes = get_btrfs_subvolumes();
	sort(btrfs_subvolumes.begin(), btrfs_subvolumes.end(), BtrfsSubvolume::compare_by_id);
	for (BtrfsSubvolume* btrfs_subvolume : btrfs_subvolumes)
	{
	    btrfs_subvolume->get_impl().probe_pass_2b(prober, mount_point);
	}
    }


    namespace
    {

	/**
	 * Auxiliary method to check whether a device is included in a list of devices.
	 */
	bool contains_device(vector<const BlkDevice*> blk_devices, const BlkDevice* blk_device)
	{
	    vector<const BlkDevice*>::iterator it = find_if(blk_devices.begin(), blk_devices.end(),
		[blk_device](const BlkDevice* dev) { return dev->get_sid() == blk_device->get_sid(); });

	    return it != blk_devices.end();
	}

    }


    ResizeInfo
    Btrfs::Impl::detect_resize_info(const BlkDevice* blk_device) const
    {
	if (blk_device && !contains_device(get_blk_devices(), blk_device))
	    ST_THROW(Exception("function called with wrong device"));

	ResizeInfo default_resize_info(true, 0, min_size(), max_size());

	if (!exists_in_system())
	{
	    // The filesystem does not exist on disk yet.

	    if (get_blk_devices().size() == 1)
	    {
		// Rely on BlkFilesystem::Impl when the filesystem is a not committed
		// single-device Btrfs.

		return BlkFilesystem::Impl::detect_resize_info();
	    }
	    else
	    {
		// The filesystem is a not committed multi-device Btrfs.

		return default_resize_info;
	    }
	}
	else
	{
	    // The filesystem already exists on disk.

	    const Btrfs* filesystem = redirect_to_system(get_non_impl());

	    if (filesystem->get_blk_devices().size() == 1)
	    {
		// Currently, the filesystem is a single-device Btrfs.

		if (blk_device && blk_device->get_sid() != filesystem->get_impl().get_blk_device()->get_sid())
		{
		    // Checking from a new device associated to the filesystem.

		    return default_resize_info;
		}
		else
		{
		    // Rely on BlkFilesystem::Impl when checking from no specific block device,
		    // or from the device currently associated to the filesystem.

		    return BlkFilesystem::Impl::detect_resize_info();
		}
	    }
	    else
	    {
		// Currently, the filesystem is a multi-device Btrfs.

		ResizeInfo resize_info(true, 0);

		if (blk_device)
		{
		    // Checking from an specific block device.

		    resize_info = filesystem->get_impl().detect_resize_info_on_disk(blk_device);
		}
		else if (!multi_device_resize_info.has_value())
		{
		    // Checking from no specific block device, and the resize info is not cached yet.

		    resize_info = filesystem->get_impl().detect_resize_info_on_disk();
		    multi_device_resize_info.set_value(resize_info);
		}
		else
		{
		    // Checking from no specific block device, and the resize info is already cached.

		    resize_info = multi_device_resize_info.get_value();
		}

		y2mil("on-disk resize-info:" << resize_info);

		return resize_info;
	    }
	}
    }


    ResizeInfo
    Btrfs::Impl::detect_resize_info_on_disk(const BlkDevice* blk_device) const
    {
	if (!get_devicegraph()->get_impl().is_system() && !get_devicegraph()->get_impl().is_probed())
	    ST_THROW(Exception("function called on wrong device"));

	// TODO btrfs provides a command to query the min size (btrfs
	// inspect-internal min-dev-size /mount-point) but it does reports
	// wrong values

	if (get_blk_devices().size() == 1)
	{
	    // Rely on BlkFilesystem::Impl for single-device Btrfs.

	    return BlkFilesystem::Impl::detect_resize_info_on_disk(blk_device);
	}

	// Multi-device Btrfs

	ResizeInfo resize_info(true, 0, min_size(), max_size());

	if (!blk_device)
	{
	    // Checking from no specific block device.

	    // Assume that the min-size, e.g. for superblocks, metadata and
	    // journal, is needed additional to the used-size.

	    resize_info.min_size += used_size_on_disk();

	    // Often the a filesystem cannot be shrunk to the value reported
	    // by statvfs. Thus add a 50% safety margin.

	    resize_info.min_size *= 1.5;

	    resize_info.reasons |= RB_SHRINK_NOT_SUPPORTED_BY_MULTIDEVICE_FILESYSTEM;

	    if (resize_info.min_size >= resize_info.max_size)
		resize_info.reasons |= RB_FILESYSTEM_FULL;
	}
	else
	{
	    // Checking from a specific block device.

	    if (contains_device(get_blk_devices(), blk_device))
	    {
		// Block shrink only if the filesystem is already using this device on the system.

		resize_info.min_size = max(resize_info.min_size, blk_device->get_size());
		resize_info.reasons |= RB_SHRINK_NOT_SUPPORTED_BY_MULTIDEVICE_FILESYSTEM;
	    }
	}

	return resize_info;
    }


    uf_t
    Btrfs::Impl::used_features(UsedFeaturesDependencyType used_features_dependency_type) const
    {
        uf_t ret = BlkFilesystem::Impl::used_features(used_features_dependency_type);

        if (configure_snapper)
            ret |= UF_SNAPSHOTS;

        return ret;
    }


    void
    Btrfs::Impl::parse_mkfs_output(const vector<string>& stdout)
    {
	static const regex uuid_regex("UUID:[ \t]+(" UUID_REGEX ")", regex::extended);

	smatch match;

	for (const string& line : stdout)
	{
	    if (regex_match(line, match, uuid_regex) && match.size() == 2)
	    {
		set_uuid(match[1]);
		return;
	    }
	}

	ST_THROW(Exception("UUID not found in output of mkfs.btrfs"));
    }


    void
    Btrfs::Impl::do_create()
    {
	string cmd_line = MKFS_BTRFS_BIN " --force";

	if (metadata_raid_level != BtrfsRaidLevel::DEFAULT)
	    cmd_line += " --metadata=" + toString(metadata_raid_level);

	if (data_raid_level != BtrfsRaidLevel::DEFAULT)
	    cmd_line += " --data=" + toString(data_raid_level);

	if (!get_uuid().empty())
	    cmd_line += " --uuid=" + quote(get_uuid());

	if (!get_mkfs_options().empty())
	    cmd_line += " " + get_mkfs_options();

	// sort is required for testsuite
	vector<const BlkDevice*> blk_devices = std::as_const(*this).get_blk_devices();
	sort(blk_devices.begin(), blk_devices.end(), BlkDevice::compare_by_name);
	for (const BlkDevice* blk_device : blk_devices)
	    cmd_line += " " + quote(blk_device->get_name());

	wait_for_devices();

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);

	if (get_uuid().empty())
	{
	    parse_mkfs_output(cmd.stdout());
	}

        // This would fit better in do_mount(), but that one is a const method
        // which would not allow to set the snapper_config member variable.
        // But we need to give the application a chance to set the
        // configure_snapper variable, so the ctor would not be good choice
        // either. This place is guaranteed to be in the commit phase, so this
        // is the best place for the time being.

        if (configure_snapper && !snapper_config)
            snapper_config = new SnapperConfig(to_btrfs(get_non_impl()));
    }


    void
    Btrfs::Impl::do_resize(const CommitData& commit_data, const Action::Resize* action) const
    {
	const BlkDevice* blk_device_rhs = action->get_resized_blk_device(commit_data.actiongraph, RHS);

	EnsureMounted ensure_mounted(get_filesystem(), false);

	const FilesystemUser* filesystem_user =
	    to_filesystem_user(get_devicegraph()->find_holder(action->blk_device->get_sid(), get_sid()));
	unsigned int devid = filesystem_user->get_impl().get_id();

	string cmd_line = BTRFS_BIN " filesystem resize " + to_string(devid) + ":";
	if (action->resize_mode == ResizeMode::SHRINK)
	    cmd_line += to_string(blk_device_rhs->get_size());
	else
	    cmd_line += "max";
	cmd_line += " " + quote(ensure_mounted.get_any_mount_point());

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);
    }


    void
    Btrfs::Impl::do_mount(CommitData& commit_data, const CommitOptions& commit_options, MountPoint* mount_point) const
    {
        if (snapper_config)
            snapper_config->pre_mount();

        BlkFilesystem::Impl::do_mount(commit_data, commit_options, mount_point);

        if (snapper_config)
            snapper_config->post_mount();
    }


    void
    Btrfs::Impl::do_add_to_etc_fstab(CommitData& commit_data, const MountPoint* mount_point) const
    {
        BlkFilesystem::Impl::do_add_to_etc_fstab(commit_data, mount_point);

        if (snapper_config)
            snapper_config->post_add_to_etc_fstab(commit_data.get_etc_fstab());
    }


    void
    Btrfs::Impl::do_set_label() const
    {
	const BlkDevice* blk_device = get_blk_device();

	// TODO handle mounted

	string cmd_line = BTRFS_BIN " filesystem label " + quote(blk_device->get_name()) + " " +
	    quote(get_label());

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);
    }


    void
    Btrfs::Impl::do_pre_mount() const
    {
	// Normally using 'btrfs device scan' is not needed but in the
	// inst-sys it seems to be required (see bsc #1096760). Better
	// be save than sorry.

	vector<const BlkDevice*> blk_devices = get_blk_devices();

	if (blk_devices.size() < 2)
	    return;

	string cmd_line = BTRFS_BIN " device scan";

	for (const BlkDevice* blk_device : blk_devices)
	    cmd_line += " " + quote(blk_device->get_name());

	wait_for_devices();

	SystemCmd cmd(cmd_line, SystemCmd::NoThrow);
    }


    Text
    Btrfs::Impl::do_reallot_text(const CommitData& commit_data, const Action::Reallot* action) const
    {
	Text text;

	switch (action->reallot_mode)
	{
	    case ReallotMode::REDUCE:
		text = tenser(commit_data.tense,
			      // TRANSLATORS: displayed before action,
			      // %1$s is replaced by the device name (e.g. /dev/sdc1),
			      // %2$s is replaced by the device size (e.g. 2.00 GiB),
			      // %3$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			      // and /dev/sdb2 (2.00 GiB))
			      _("Remove %1$s (%2$s) from btrfs on %3$s"),
			      // TRANSLATORS: displayed during action,
			      // %1$s is replaced by the device name (e.g. /dev/sdc1),
			      // %2$s is replaced by the device size (e.g. 2.00 GiB),
			      // %3$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			      // and /dev/sdb2 (2.00 GiB))
			      _("Removing %1$s (%2$s) from btrfs on %3$s"));
		break;

	    case ReallotMode::EXTEND:
		text = tenser(commit_data.tense,
			      // TRANSLATORS: displayed before action,
			      // %1$s is replaced by the device name (e.g. /dev/sdc1),
			      // %2$s is replaced by the device size (e.g. 2.00 GiB),
			      // %3$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			      // and /dev/sdb2 (2.00 GiB))
			      _("Add %1$s (%2$s) to btrfs on %3$s"),
			      // TRANSLATORS: displayed during action,
			      // %1$s is replaced by the device name (e.g. /dev/sdc1),
			      // %2$s is replaced by the device size (e.g. 2.00 GiB),
			      // %3$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			      // and /dev/sdb2 (2.00 GiB))
			      _("Adding %1$s (%2$s) to btrfs on %3$s"));
		break;

	    default:
		ST_THROW(LogicException("invalid value for reallot_mode"));
	}

	const BlkDevice* blk_device = to_blk_device(action->device);

	return sformat(text, blk_device->get_name(), blk_device->get_size_string(),
		       get_message_name());
    }


    void
    Btrfs::Impl::do_reallot(const CommitData& commit_data, const Action::Reallot* action) const
    {
	const BlkDevice* blk_device = to_blk_device(action->device);

	switch (action->reallot_mode)
	{
	    case ReallotMode::REDUCE:
		do_reduce(blk_device);
		return;

	    case ReallotMode::EXTEND:
		do_extend(blk_device);
		return;
	}

	ST_THROW(LogicException("invalid value for reallot_mode"));
    }


    void
    Btrfs::Impl::do_reduce(const BlkDevice* blk_device) const
    {
	// TODO EnsureMounted always works on the system devicegraph. This
	// could fail if reduce and extend actions are run in sequence
	// (but this is so far not supported).

	EnsureMounted ensure_mounted(get_filesystem(), false);

	string cmd_line = BTRFS_BIN " device remove " + quote(blk_device->get_name()) + " " +
	    quote(ensure_mounted.get_any_mount_point());

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);
    }


    void
    Btrfs::Impl::do_extend(const BlkDevice* blk_device) const
    {
	// TODO See do_reduce above.

	EnsureMounted ensure_mounted(get_filesystem(), false);

	string cmd_line = BTRFS_BIN " device add " + quote(blk_device->get_name()) + " " +
	    quote(ensure_mounted.get_any_mount_point());

	storage::wait_for_devices({ blk_device });

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);
    }


    Text
    Btrfs::Impl::do_set_quota_text(const CommitData& commit_data, const Action::SetQuota* action) const
    {
	Text text;

	if (quota)
	    text = tenser(commit_data.tense,
			  // TRANSLATORS: displayed before action,
			  // %1$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			  // and /dev/sdb2 (2.00 GiB))
			  _("Enable quota on %1$s"),
			  // TRANSLATORS: displayed during action,
			  // %1$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			  // and /dev/sdb2 (2.00 GiB))
			  _("Enabling quota %1$s"));
	else
	    text = tenser(commit_data.tense,
			  // TRANSLATORS: displayed before action,
			  // %1$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			  // and /dev/sdb2 (2.00 GiB))
			  _("Disable quota on %1$s"),
			  // TRANSLATORS: displayed during action,
			  // %1$s is replaced by one or more devices (e.g /dev/sda1 (2.00 GiB)
			  // and /dev/sdb2 (2.00 GiB))
			  _("Disabling quota on %1$s"));

	return sformat(text, get_message_name());
    }


    void
    Btrfs::Impl::do_set_quota(const CommitData& commit_data, const Action::SetQuota* action) const
    {
	EnsureMounted ensure_mounted(get_top_level_btrfs_subvolume(), false);

	string cmd_line = BTRFS_BIN " quota " + string(quota ? "enable" : "disable") + " " +
	    quote(ensure_mounted.get_any_mount_point());

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);
    }


    void
    Btrfs::Impl::add_dependencies(Actiongraph::Impl& actiongraph) const
    {
	BlkFilesystem::Impl::add_dependencies(actiongraph);

	// So far only actions that increase the overall size of a
	// multiple devices btrfs are supported. Thus no actions need
	// to be ordered here.
    }

}
