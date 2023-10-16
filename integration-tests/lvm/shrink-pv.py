#!/usr/bin/python3

# requirements: lvm vg test with lvm pv on /dev/sdc1


from sys import exit
from storage import *
from storageitu import *


set_logger(get_logfile_logger())

environment = Environment(False)

storage = Storage(environment)
storage.probe()

staging = storage.get_staging()

print(staging)

sdc1 = BlkDevice.find_by_name(staging, "/dev/sdc1")

sdc1.set_size(sdc1.get_size() - 512 * MiB)

print(staging)

commit(storage)

