/*
 * Copyright (c) 2024 SUSE LLC
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


#include "storage/Filesystems/BcachefsImpl.h"
#include "storage/Devicegraph.h"


namespace storage
{

    using namespace std;


    Bcachefs*
    Bcachefs::create(Devicegraph* devicegraph)
    {
	shared_ptr<Bcachefs> bcachefs = make_shared<Bcachefs>(new Bcachefs::Impl());
	Device::Impl::create(devicegraph, bcachefs);
	return bcachefs.get();
    }


    Bcachefs*
    Bcachefs::load(Devicegraph* devicegraph, const xmlNode* node)
    {
	shared_ptr<Bcachefs> bcachefs = make_shared<Bcachefs>(new Bcachefs::Impl(node));
	Device::Impl::load(devicegraph, bcachefs);
	return bcachefs.get();
    }


    Bcachefs::Bcachefs(Impl* impl)
	: BlkFilesystem(impl)
    {
    }


    Bcachefs*
    Bcachefs::clone() const
    {
	return new Bcachefs(get_impl().clone());
    }


    Bcachefs::Impl&
    Bcachefs::get_impl()
    {
	return dynamic_cast<Impl&>(Device::get_impl());
    }


    const Bcachefs::Impl&
    Bcachefs::get_impl() const
    {
	return dynamic_cast<const Impl&>(Device::get_impl());
    }


    bool
    is_bcachefs(const Device* device)
    {
	return is_device_of_type<const Bcachefs>(device);
    }


    Bcachefs*
    to_bcachefs(Device* device)
    {
	return to_device_of_type<Bcachefs>(device);
    }


    const Bcachefs*
    to_bcachefs(const Device* device)
    {
	return to_device_of_type<const Bcachefs>(device);
    }

}
