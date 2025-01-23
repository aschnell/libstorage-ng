#!/usr/bin/python3

# requirements: empty partitions sdc1 and sdd1


from sys import exit
from storage import *
from storageitu import *


set_logger(get_logfile_logger())

environment = Environment(False)

storage = Storage(environment)
storage.probe()

staging = storage.get_staging()

sdc1 = Partition.find_by_name(staging, "/dev/sdc1")
sdd1 = Partition.find_by_name(staging, "/dev/sdd1")

test = LvmVg.create(staging, "test")
test.add_lvm_pv(sdc1)
test.add_lvm_pv(sdd1)
test.set_extent_size(128 * MiB)

print(staging)

commit(storage)

