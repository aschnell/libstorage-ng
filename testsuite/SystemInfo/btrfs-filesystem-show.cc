
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE libstorage

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "storage/SystemInfo/CmdBtrfs.h"
#include "storage/Utils/Mockup.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/StorageDefines.h"


using namespace std;
using namespace storage;


void
check(const vector<string>& input, const vector<string>& output)
{
    Mockup::set_mode(Mockup::Mode::PLAYBACK);
    Mockup::set_command(BTRFS_BIN " filesystem show", input);
    Mockup::set_command({ UDEVADM_BIN_SETTLE }, {});

    Udevadm udevadm;

    CmdBtrfsFilesystemShow cmd_btrfs_filesystem_show(udevadm);

    ostringstream parsed;
    parsed.setf(std::ios::boolalpha);
    parsed << cmd_btrfs_filesystem_show;

    string lhs = parsed.str();
    string rhs;

    if ( !output.empty() )
	rhs = boost::join(output, "\n") + "\n";

    BOOST_CHECK_EQUAL(lhs, rhs);
}


void
check_parse_exception(const vector<string>& input)
{
    Mockup::set_mode(Mockup::Mode::PLAYBACK);
    Mockup::set_command(BTRFS_BIN " filesystem show", input);

    Udevadm udevadm;

    BOOST_CHECK_THROW({ CmdBtrfsFilesystemShow cmd_btrfs_filesystem_show(udevadm); }, ParseException);
}


void
check_systemcmd_exception(const vector<string>& input, const vector<string>& stderr)
{
    Mockup::set_mode(Mockup::Mode::PLAYBACK);
    // "input" is (mocked) "stdout" of command
    Mockup::Command command(input, stderr, 1);
    Mockup::set_command(BTRFS_BIN " filesystem show", command);

    Udevadm udevadm;

    BOOST_CHECK_THROW({ CmdBtrfsFilesystemShow cmd_btrfs_filesystem_show(udevadm); }, SystemCmdException);
}


BOOST_AUTO_TEST_CASE(parse_good)
{
    vector<string> input = {
	"Label: none  uuid: ea108250-d02c-41dd-b4d8-d4a707a5c649",
	"        Total devices 1 FS bytes used 28.00KiB",
	"        devid    1 size 1.00GiB used 138.38MiB path /dev/mapper/system-test",
	"",
	"Label: none  uuid: d82229f2-f9e4-40fd-b15f-84e2d42e6d0d",
	"        Total devices 1 FS bytes used 420.00KiB",
	"        devid    1 size 2.00GiB used 240.75MiB path /dev/mapper/system-testsuite",
	"",
	"Label: none  uuid: 653764e0-7ea2-4dbe-9fa1-866f3f7783c9",
	"        Total devices 1 FS bytes used 316.00KiB",
	"        devid    1 size 5.00GiB used 548.00MiB path /dev/mapper/system-btrfs",
	"",
	"Btrfs v3.12+20131125"
    };

    vector<string> output = {
	"uuid:ea108250-d02c-41dd-b4d8-d4a707a5c649 devices:<{ id:1 name:/dev/mapper/system-test }>",
	"uuid:d82229f2-f9e4-40fd-b15f-84e2d42e6d0d devices:<{ id:1 name:/dev/mapper/system-testsuite }>",
	"uuid:653764e0-7ea2-4dbe-9fa1-866f3f7783c9 devices:<{ id:1 name:/dev/mapper/system-btrfs }>"
    };

    check(input, output);
}


BOOST_AUTO_TEST_CASE(parse_empty)
{
    // Sample output if there is no btrfs filesystem at all on the system
    vector<string> input = {
	"Btrfs v3.12+20131125"
    };

    vector<string> output;

    check(input, output);
}


BOOST_AUTO_TEST_CASE(parse_bad_device_name)
{
    vector<string> input = {
	"Label: none  uuid: ea108250-d02c-41dd-b4d8-d4a707a5c649",
	"        Total devices 1 FS bytes used 28.00KiB",
	"        devid    1 size 1.00GiB used 138.38MiB path notadevicename", // no /dev/...
	"",
	"Btrfs v3.12+20131125"
    };

    check_parse_exception(input);
}


BOOST_AUTO_TEST_CASE(parse_no_devices)
{
    vector<string> input = {
	"Label: none  uuid: ea108250-d02c-41dd-b4d8-d4a707a5c649",
	"        Total devices 1 FS bytes used 28.00KiB",
	"",
	"Btrfs v3.12+20131125"
    };

    check_parse_exception(input);
}


BOOST_AUTO_TEST_CASE(systemcmd_error)
{
    vector<string> input = {
	"Label: none  uuid: 653764e0-7ea2-4dbe-9fa1-866f3f7783c9",
	"        Total devices 1 FS bytes used 316.00KiB",
	"        devid    1 size 5.00GiB used 548.00MiB path /dev/mapper/system-btrfs",
	"",
	"Btrfs v3.12+20131125"
    };

    vector<string> stderr = { "Unknown error..." };

    check_systemcmd_exception(input, stderr);
}


BOOST_AUTO_TEST_CASE(parse_missing)
{
    vector<string> input = {
	"Label: 'hello world'  uuid: b0749dbe-7de5-4719-9cb6-043dd5c70d00",
	"        Total devices 4 FS bytes used 256.00KiB",
	"        devid    1 size 2.00GiB used 417.12MiB path /dev/sdb1",
	"        devid    2 size 2.00GiB used 417.12MiB path /dev/sdc1",
	"        devid    3 size 2.00GiB used 417.12MiB path /dev/sdd1",
	"        *** Some devices missing",
	""
    };

    vector<string> output = {
	"uuid:b0749dbe-7de5-4719-9cb6-043dd5c70d00 devices:<{ id:1 name:/dev/sdb1 } { id:2 name:/dev/sdc1 } { id:3 name:/dev/sdd1 }>"
    };

    check(input, output);
}
