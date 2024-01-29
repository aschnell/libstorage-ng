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


#ifndef STORAGE_BCACHEFS_IMPL_H
#define STORAGE_BCACHEFS_IMPL_H


#include "storage/Filesystems/Bcachefs.h"
#include "storage/Filesystems/BlkFilesystemImpl.h"
#include "storage/Utils/HumanString.h"
#include "storage/UsedFeatures.h"


namespace storage
{

    using namespace std;


    template <> struct DeviceTraits<Bcachefs> { static const char* classname; };


    class Bcachefs::Impl : public BlkFilesystem::Impl
    {

    public:

	virtual unsigned long long min_size() const override { return 16 * MiB; }
	virtual unsigned long long max_size() const override { return 8 * EiB - 1 * B; }

	virtual bool supports_mounted_shrink() const override { return false; }
	virtual bool supports_mounted_grow() const override { return false; }

	virtual bool supports_unmounted_shrink() const override { return false; }
	virtual bool supports_unmounted_grow() const override { return true; }

	virtual bool supports_label() const override { return true; }
	virtual bool supports_modify_label() const override { return false; }
	virtual unsigned int max_labelsize() const override { return 32; }

	virtual bool supports_uuid() const override { return true; }
	virtual bool supports_modify_uuid() const override { return false; }

    public:

	Impl()
	    : BlkFilesystem::Impl() {}

	Impl(const xmlNode* node);

	virtual FsType get_type() const override { return FsType::BCACHEFS; }

	virtual const char* get_classname() const override { return DeviceTraits<Bcachefs>::classname; }

	virtual string get_pretty_classname() const override;

	virtual string get_displayname() const override { return "bcachefs"; }

	virtual Impl* clone() const override { return new Impl(*this); }

	virtual uf_t used_features_pure() const override { return UF_BCACHEFS; }

	virtual void do_create() override;

    };

}

#endif
