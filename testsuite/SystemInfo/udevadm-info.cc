
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE libstorage

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "storage/SystemInfo/CmdUdevadm.h"
#include "storage/Utils/Mockup.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/StorageDefines.h"


using namespace std;
using namespace storage;


void
check(const string& file, const vector<string>& input, const vector<string>& output)
{
    Mockup::set_mode(Mockup::Mode::PLAYBACK);
    Mockup::set_command({ UDEVADM_BIN_SETTLE }, {});
    Mockup::set_command(UDEVADM_BIN " info " + quote(file), input);

    CmdUdevadmInfo cmd_udevadm_info(file);

    ostringstream parsed;
    parsed.setf(std::ios::boolalpha);
    parsed << cmd_udevadm_info;

    string lhs = parsed.str();
    string rhs = boost::join(output, "\n") + "\n";

    BOOST_CHECK_EQUAL(lhs, rhs);
}


BOOST_AUTO_TEST_CASE(parse1)
{
    vector<string> input = {
	"P: /devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda",
	"N: sda",
	"S: disk/by-id/ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"S: disk/by-id/scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683",
	"S: disk/by-id/scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"S: disk/by-id/scsi-350014ee203733bb5",
	"S: disk/by-id/scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683",
	"S: disk/by-id/wwn-0x50014ee203733bb5",
	"S: disk/by-path/pci-0000:00:1f.2-ata-1",
	"E: DEVLINKS=/dev/disk/by-id/ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683 /dev/disk/by-path/pci-0000:00:1f.2-ata-1 /dev/disk/by-id/wwn-0x50014ee203733bb5 /dev/disk/by-id/scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683 /dev/disk/by-id/scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683 /dev/disk/by-id/scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683 /dev/disk/by-id/scsi-350014ee203733bb5",
	"E: DEVNAME=/dev/sda",
	"E: DEVPATH=/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda",
	"E: DEVTYPE=disk",
	"E: DONT_DEL_PART_NODES=1",
	"E: ID_ATA=1",
	"E: ID_ATA_DOWNLOAD_MICROCODE=1",
	"E: ID_ATA_FEATURE_SET_AAM=1",
	"E: ID_ATA_FEATURE_SET_AAM_CURRENT_VALUE=254",
	"E: ID_ATA_FEATURE_SET_AAM_ENABLED=0",
	"E: ID_ATA_FEATURE_SET_AAM_VENDOR_RECOMMENDED_VALUE=128",
	"E: ID_ATA_FEATURE_SET_HPA=1",
	"E: ID_ATA_FEATURE_SET_HPA_ENABLED=1",
	"E: ID_ATA_FEATURE_SET_PM=1",
	"E: ID_ATA_FEATURE_SET_PM_ENABLED=1",
	"E: ID_ATA_FEATURE_SET_PUIS=1",
	"E: ID_ATA_FEATURE_SET_PUIS_ENABLED=0",
	"E: ID_ATA_FEATURE_SET_SECURITY=1",
	"E: ID_ATA_FEATURE_SET_SECURITY_ENABLED=0",
	"E: ID_ATA_FEATURE_SET_SECURITY_ENHANCED_ERASE_UNIT_MIN=216",
	"E: ID_ATA_FEATURE_SET_SECURITY_ERASE_UNIT_MIN=216",
	"E: ID_ATA_FEATURE_SET_SECURITY_FROZEN=1",
	"E: ID_ATA_FEATURE_SET_SMART=1",
	"E: ID_ATA_FEATURE_SET_SMART_ENABLED=1",
	"E: ID_ATA_SATA=1",
	"E: ID_ATA_SATA_SIGNAL_RATE_GEN1=1",
	"E: ID_ATA_SATA_SIGNAL_RATE_GEN2=1",
	"E: ID_ATA_WRITE_CACHE=1",
	"E: ID_ATA_WRITE_CACHE_ENABLED=1",
	"E: ID_BUS=ata",
	"E: ID_MODEL=WDC_WD10EADS-00M2B0",
	"E: ID_MODEL_ENC=WDC\x20WD10EADS-00M2B0\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20",
	"E: ID_PART_TABLE_TYPE=dos",
	"E: ID_PART_TABLE_UUID=000c0a5a",
	"E: ID_PATH=pci-0000:00:1f.2-ata-1",
	"E: ID_PATH_TAG=pci-0000_00_1f_2-ata-1",
	"E: ID_REVISION=01.00A01",
	"E: ID_SCSI=1",
	"E: ID_SCSI_INQUIRY=1",
	"E: ID_SERIAL=WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: ID_SERIAL_SHORT=WD-WCAV52321683",
	"E: ID_TYPE=disk",
	"E: ID_VENDOR=ATA",
	"E: ID_VENDOR_ENC=ATA\x20\x20\x20\x20\x20",
	"E: ID_WWN=0x50014ee203733bb5",
	"E: ID_WWN_WITH_EXTENSION=0x50014ee203733bb5",
	"E: MAJOR=8",
	"E: MINOR=0",
	"E: MPATH_SBIN_PATH=/sbin",
	"E: SCSI_IDENT_LUN_ATA=WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: SCSI_IDENT_LUN_NAA_REG=50014ee203733bb5",
	"E: SCSI_IDENT_LUN_T10=ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: SCSI_IDENT_LUN_VENDOR=WD-WCAV52321683",
	"E: SCSI_IDENT_SERIAL=WD-WCAV52321683",
	"E: SCSI_MODEL=WDC_WD10EADS-00M",
	"E: SCSI_MODEL_ENC=WDC\x20WD10EADS-00M",
	"E: SCSI_REVISION=0A01",
	"E: SCSI_TPGS=0",
	"E: SCSI_TYPE=disk",
	"E: SCSI_VENDOR=ATA",
	"E: SCSI_VENDOR_ENC=ATA\x20\x20\x20\x20\x20",
	"E: SUBSYSTEM=block",
	"E: TAGS=:systemd:",
	"E: USEC_INITIALIZED=30039765",
	"E: elevator=cfq",
	""
    };

    vector<string> output = {
	"file:/dev/sda path:/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda name:sda majorminor:8:0 device-type:disk by-path-links:<pci-0000:00:1f.2-ata-1> by-id-links:<ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683 scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683 scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683 scsi-350014ee203733bb5 scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683 wwn-0x50014ee203733bb5>"
    };

    check("/dev/sda", input, output);
}


BOOST_AUTO_TEST_CASE(parse2)
{
    vector<string> input = {
	"P: /devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1",
	"N: sda1",
	"S: disk/by-id/ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1",
	"S: disk/by-id/scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1",
	"S: disk/by-id/scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1",
	"S: disk/by-id/scsi-350014ee203733bb5-part1",
	"S: disk/by-id/scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1",
	"S: disk/by-id/scsi-SATA_WDC_WD10EADS-00_WD-WCAV52321683-part1",
	"S: disk/by-id/wwn-0x50014ee203733bb5-part1",
	"S: disk/by-label/BOOT",
	"S: disk/by-path/pci-0000:00:1f.2-ata-1.0-part1",
	"S: disk/by-uuid/14875716-b8e3-4c83-ac86-48c20682b63a",
	"E: DEVLINKS=/dev/disk/by-id/ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1 /dev/disk/by-id/scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1 /dev/disk/by-id/scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1 /dev/disk/by-id/scsi-350014ee203733bb5-part1 /dev/disk/by-id/scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1 /dev/disk/by-id/scsi-SATA_WDC_WD10EADS-00_WD-WCAV52321683-part1 /dev/disk/by-id/wwn-0x50014ee203733bb5-part1 /dev/disk/by-label/BOOT /dev/disk/by-path/pci-0000:00:1f.2-ata-1.0-part1 /dev/disk/by-uuid/14875716-b8e3-4c83-ac86-48c20682b63a",
	"E: DEVNAME=/dev/sda1",
	"E: DEVPATH=/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1",
	"E: DEVTYPE=partition",
	"E: ID_ATA=1",
	"E: ID_BUS=ata",
	"E: ID_FS_LABEL=BOOT",
	"E: ID_FS_LABEL_ENC=BOOT",
	"E: ID_FS_TYPE=ext3",
	"E: ID_FS_USAGE=filesystem",
	"E: ID_FS_UUID=14875716-b8e3-4c83-ac86-48c20682b63a",
	"E: ID_FS_UUID_ENC=14875716-b8e3-4c83-ac86-48c20682b63a",
	"E: ID_FS_VERSION=1.0",
	"E: ID_MODEL=WDC_WD10EADS-00M",
	"E: ID_MODEL_ENC=WDC\x20WD10EADS-00M",
	"E: ID_PART_ENTRY_DISK=8:0",
	"E: ID_PART_ENTRY_FLAGS=0x80",
	"E: ID_PART_ENTRY_NUMBER=1",
	"E: ID_PART_ENTRY_OFFSET=2048",
	"E: ID_PART_ENTRY_SCHEME=dos",
	"E: ID_PART_ENTRY_SIZE=2103296",
	"E: ID_PART_ENTRY_TYPE=0x83",
	"E: ID_PART_ENTRY_UUID=000c0a5a-01",
	"E: ID_PART_TABLE_TYPE=dos",
	"E: ID_PART_TABLE_UUID=000c0a5a",
	"E: ID_PATH=pci-0000:00:1f.2-ata-1.0",
	"E: ID_PATH_TAG=pci-0000_00_1f_2-ata-1_0",
	"E: ID_REVISION=0A01",
	"E: ID_SCSI=1",
	"E: ID_SCSI_COMPAT=SATA_WDC_WD10EADS-00M_WD-WCAV52321683",
	"E: ID_SCSI_COMPAT_TRUNCATED=SATA_WDC_WD10EADS-00_WD-WCAV52321683",
	"E: ID_SERIAL=WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: ID_SERIAL_SHORT=WD-WCAV52321683",
	"E: ID_TYPE=disk",
	"E: ID_VENDOR=ATA",
	"E: ID_VENDOR_ENC=ATA\x20\x20\x20\x20\x20",
	"E: ID_WWN=0x50014ee203733bb5",
	"E: ID_WWN_WITH_EXTENSION=0x50014ee203733bb5",
	"E: MAJOR=8",
	"E: MINOR=1",
	"E: MPATH_SBIN_PATH=/sbin",
	"E: SCSI_IDENT_LUN_ATA=WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: SCSI_IDENT_LUN_NAA=50014ee203733bb5",
	"E: SCSI_IDENT_LUN_T10=ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683",
	"E: SCSI_IDENT_LUN_VENDOR=WD-WCAV52321683",
	"E: SCSI_IDENT_SERIAL=WD-WCAV52321683",
	"E: SCSI_MODEL=WDC_WD10EADS-00M",
	"E: SCSI_MODEL_ENC=WDC\x20WD10EADS-00M",
	"E: SCSI_REVISION=0A01",
	"E: SCSI_TPGS=0",
	"E: SCSI_TYPE=disk",
	"E: SCSI_VENDOR=ATA",
	"E: SCSI_VENDOR_ENC=ATA\x20\x20\x20\x20\x20",
	"E: SUBSYSTEM=block",
	"E: TAGS=:systemd:",
	"E: USEC_INITIALIZED=759018",
	""
    };

    vector<string> output = {
	"file:/dev/sda1 path:/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda/sda1 name:sda1 majorminor:8:1 device-type:partition by-path-links:<pci-0000:00:1f.2-ata-1.0-part1> by-id-links:<ata-WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1 scsi-0ATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1 scsi-1ATA_WDC_WD10EADS-00M2B0_WD-WCAV52321683-part1 scsi-350014ee203733bb5-part1 scsi-SATA_WDC_WD10EADS-00M_WD-WCAV52321683-part1 scsi-SATA_WDC_WD10EADS-00_WD-WCAV52321683-part1 wwn-0x50014ee203733bb5-part1> by-label-links:<BOOT> by-uuid-links:<14875716-b8e3-4c83-ac86-48c20682b63a>"
    };

    check("/dev/sda1", input, output);
}
