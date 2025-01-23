#!/usr/bin/python3

# requirements: disk /dev/sdc with plain encryption on partition sdc1


from sys import exit
from storage import *
from storageitu import *


set_logger(get_logfile_logger())

environment = Environment(False)

storage = Storage(environment)
storage.probe()

staging = storage.get_staging()

sdc1 = Partition.find_by_name(staging, "/dev/sdc1")

sdc1.remove_descendants(View_REMOVE)

print(staging)

commit(storage)

