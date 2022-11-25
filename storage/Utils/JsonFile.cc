/*
 * Copyright (c) [2017-2022] SUSE LLC
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


#include <stdio.h>
#include <sys/stat.h>
#include <functional>
#include <memory>
#include <sstream>

#include "storage/Utils/JsonFile.h"
#include "storage/Utils/ExceptionImpl.h"
#include "storage/Utils/AppUtil.h"
#include "storage/Utils/Format.h"


namespace storage
{

    JsonFile::JsonFile(const vector<string>& lines)
    {
	std::unique_ptr<json_tokener, std::function<void(json_tokener*)>> tokener(
	    json_tokener_new(), [](json_tokener* p) { json_tokener_free(p); }
	);

	for (const string& line : lines)
	{
	    root = json_tokener_parse_ex(tokener.get(), line.c_str(), line.size());

	    switch (json_tokener_get_error(tokener.get()))
	    {
		case json_tokener_continue:
		    continue;

		case json_tokener_success:
		    return;

		default:
		    break;
	    }
	}

	ST_THROW(Exception("json parser failed"));
    }


    JsonFile::JsonFile(const string& filename)
    {
	FILE* fp = fopen(filename.c_str(), "r");
	if (!fp)
	    ST_THROW(IOException(sformat("open for json file '%s' failed", filename)));

	struct stat st;
	if (fstat(fileno(fp), &st) != 0)
	{
	    fclose(fp);
	    ST_THROW(IOException(sformat("stat for json file '%s' failed", filename)));
	}

	vector<char> data(st.st_size);
	if (fread(data.data(), 1, st.st_size, fp) != (size_t)(st.st_size))
	{
	    fclose(fp);
	    ST_THROW(Exception(sformat("read for json file '%s' failed", filename)));
	}

	if (fclose(fp) != 0)
	{
	    ST_THROW(Exception(sformat("close for json file '%s' failed", filename)));
	}

	json_tokener* tokener = json_tokener_new();

	root = json_tokener_parse_ex(tokener, data.data(), st.st_size);

	if (json_tokener_get_error(tokener) != json_tokener_success)
	{
	    json_tokener_free(tokener);
	    json_object_put(root);
	    ST_THROW(Exception(sformat("parsing json file '%s' failed", filename)));
	}

	if (tokener->char_offset != st.st_size)
	{
	    json_tokener_free(tokener);
	    json_object_put(root);
	    ST_THROW(Exception(sformat("excessive content in json file '%s'", filename)));
	}

	json_tokener_free(tokener);
    }


    JsonFile::~JsonFile()
    {
	json_object_put(root);
    }


    template<>
    bool
    get_child_value(json_object* parent, const char* name, string& value)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_string))
	    return false;

	value = json_object_get_string(child);

	return true;
    }


    template<>
    bool
    get_child_value(json_object* parent, const char* name, int& value)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_int))
	    return false;

	value = json_object_get_int(child);

	return true;
    }


    template<>
    bool
    get_child_value(json_object* parent, const char* name, unsigned int& value)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_int))
	    return false;

	value = json_object_get_int(child);

	return true;
    }


    template<>
    bool
    get_child_value(json_object* parent, const char* name, unsigned long& value)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_string))
	    return false;

	std::istringstream istr(json_object_get_string(child));
	classic(istr);
	istr >> value;

	return true;
    }


    template<>
    bool
    get_child_value(json_object* parent, const char* name, unsigned long long& value)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_string))
	    return false;

	std::istringstream istr(json_object_get_string(child));
	classic(istr);
	istr >> value;

	return true;
    }


    bool
    get_child_node(json_object* parent, const char* name, json_object*& child)
    {
	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_object))
	    return false;

	return true;
    }


    bool
    get_child_nodes(json_object* parent, const char* name, vector<json_object*>& children)
    {
	json_object* child;

	if (!json_object_object_get_ex(parent, name, &child))
	    return false;

	if (!json_object_is_type(child, json_type_array))
	    return false;

	children.clear();

	size_t s = json_object_array_length(child);
	for (size_t i = 0; i < s; ++i)
	    children.push_back(json_object_array_get_idx(child, i));

	return true;
    }

}
