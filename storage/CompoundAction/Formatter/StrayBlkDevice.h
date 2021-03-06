/*
 * Copyright (c) 2018 SUSE LLC
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
 * with this program; if not, contact SUSE LLC.
 *
 * To contact SUSE LLC about this file by physical or electronic mail, you may
 * find current contact information at www.suse.com.
 */


#ifndef STORAGE_FORMATTER_STRAY_BLK_DEVICE_H
#define STORAGE_FORMATTER_STRAY_BLK_DEVICE_H


#include "storage/CompoundAction/Formatter.h"
#include "storage/Devices/StrayBlkDevice.h"
#include "storage/Filesystems/BlkFilesystem.h"


namespace storage
{

    class CompoundAction::Formatter::StrayBlkDevice : public CompoundAction::Formatter
    {

    public:

	StrayBlkDevice( const CompoundAction::Impl* compound_action );

	virtual Text text() const override;

    private:

	Text format_as_swap_text() const;
	Text format_as_encrypted_swap_text() const;
	Text encrypted_pv_text() const;
	Text pv_text() const;

	Text encrypted_with_fs_and_mount_point_text() const;
	Text encrypted_with_fs_text() const;
	Text encrypted_text() const;
	Text fs_and_mount_point_text() const;
	Text fs_text() const;
	Text mount_point_text() const;

	string get_device_name() const { return stray_blk_device->get_name();	     }
	string get_size()	 const { return stray_blk_device->get_size_string(); }

    private:

	const storage::StrayBlkDevice* stray_blk_device = nullptr;

    };

}

#endif
