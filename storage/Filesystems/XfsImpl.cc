/*
 * Copyright (c) 2015 Novell, Inc.
 * Copyright (c) [2016-2023] SUSE LLC
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
#include "storage/Holders/FilesystemUser.h"
#include "storage/Filesystems/XfsImpl.h"
#include "storage/Filesystems/MountPoint.h"
#include "storage/Devicegraph.h"
#include "storage/Utils/StorageDefines.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/UsedFeatures.h"
#include "storage/SystemInfo/SystemInfoImpl.h"
#include "storage/Prober.h"


namespace storage
{

    using namespace std;


    const char* DeviceTraits<Xfs>::classname = "Xfs";


    Xfs::Impl::Impl(const xmlNode* node)
	: BlkFilesystem::Impl(node)
    {
    }


    uf_t
    Xfs::Impl::used_features_pure(const MountPoint* mount_point) const
    {
	static const regex rx1("(u|usr||g|grp|p|prj)quota", regex::extended);
	static const regex rx2("(u||g|p)qnoenforce", regex::extended);

	for (const string& mount_option : mount_point->get_mount_options())
	{
	    if (regex_match(mount_option, rx1) || regex_match(mount_option, rx2))
		return UF_QUOTA;
	}

	return 0;
    }


    string
    Xfs::Impl::get_pretty_classname() const
    {
	// TRANSLATORS: name of object
	return _("XFS").translated;
    }


    void
    Xfs::Impl::probe_pass_2b(Prober& prober)
    {
	BlkFilesystem::Impl::probe_pass_2b(prober);

	if (!get_uuid().empty())
	{
	    const Blkid& blkid = prober.get_system_info().getBlkid();
	    Blkid::const_iterator it = blkid.find_by_journal_uuid(get_uuid());
	    if (it != blkid.end())
	    {
		BlkDevice* jbd = BlkDevice::Impl::find_by_any_name(prober.get_system(), it->first,
								   prober.get_system_info());
		FilesystemUser* filesystem_user = FilesystemUser::create(prober.get_system(), jbd,
									 get_non_impl());
		filesystem_user->set_journal(true);
	    }
	}
    }


    void
    Xfs::Impl::do_create()
    {
	const BlkDevice* blk_device = get_blk_device();

	string cmd_line = MKFS_XFS_BIN " -q -f " + get_mkfs_options() + " " +
	    quote(blk_device->get_name());

	wait_for_devices();

	SystemCmd cmd(cmd_line, SystemCmd::DoThrow);

	if (get_uuid().empty())
	{
	    probe_uuid();
	}
    }


    void
    Xfs::Impl::do_resize(const CommitData& commit_data, const Action::Resize* action) const
    {
	if (action->resize_mode == ResizeMode::SHRINK)
	    ST_THROW(Exception("shrink Xfs not possible"));

	EnsureMounted ensure_mounted(get_filesystem(), false);

	SystemCmd::Args cmd_args = { XFSGROWFS_BIN, ensure_mounted.get_any_mount_point() };

	SystemCmd cmd(cmd_args, SystemCmd::DoThrow);
    }


    void
    Xfs::Impl::do_set_label() const
    {
	const BlkDevice* blk_device = get_blk_device();

	SystemCmd::Args cmd_args = { XFSADMIN_BIN, "-L", get_label().empty() ? "--" : get_label(),
	    blk_device->get_name() };

	SystemCmd cmd(cmd_args, SystemCmd::DoThrow);
    }


    void
    Xfs::Impl::do_set_uuid() const
    {
	const BlkDevice* blk_device = get_blk_device();

	SystemCmd::Args cmd_args = { XFSADMIN_BIN, "-U", get_uuid(), blk_device->get_name() };

	SystemCmd cmd(cmd_args, SystemCmd::DoThrow);
    }

}
