#!/usr/bin/python3

# requirements: md raid md0


from storage import *
from storageitu import *


set_logger(get_logfile_logger())

environment = Environment(False)

storage = Storage(environment)
storage.probe()

staging = storage.get_staging()

md0 = Md.find_by_name(staging, "/dev/md0")

md0.remove_descendants(View_REMOVE)
staging.remove_device(md0)

print(staging)

commit(storage)

