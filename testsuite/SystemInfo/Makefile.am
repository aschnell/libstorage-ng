#
# Makefile.am for libstorage/testsuite/SystemInfo
#

AM_CPPFLAGS = -I$(top_srcdir)

LDADD = ../../storage/libstorage-ng.la -lboost_unit_test_framework

check_PROGRAMS =								\
	blkid.test btrfs-filesystem-df-60.test btrfs-filesystem-df-61.test 	\
	btrfs-filesystem-show.test 						\
	btrfs-subvolume-get-default.test btrfs-subvolume-list.test		\
	btrfs-subvolume-show.test btrfs-qgroup-show-60.test 			\
	btrfs-qgroup-show-602.test btrfs-qgroup-show-62.test			\
	cryptsetup-status.test cryptsetup-bitlk-dump.test			\
	cryptsetup-luks-dump.test dasdview.test df.test 			\
	dir.test dmraid.test dumpe2fs.test resize2fs.test ntfsresize.test	\
	dmsetup-info.test dmsetup-table.test lsattr.test lsscsi.test lvs.test	\
	mdadm-detail.test mdlinks.test						\
	parted-34.test parted-35.test						\
	proc-mdstat.test proc-mounts.test pvs.test systeminfo.test		\
	udevadm-info.test vgs.test multipath.test nvme-list.test		\
	nvme-list-subsys.test

AM_DEFAULT_SOURCE_EXT = .cc

TESTS = $(check_PROGRAMS)

