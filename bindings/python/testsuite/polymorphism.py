#!/usr/bin/python

import unittest
import storage


class TestPolymorphism(unittest.TestCase):

    def test_polymorphism(self):

      devicegraph = storage.Devicegraph()
      sda = storage.Disk.create(devicegraph, "/dev/sda")
      gpt = sda.create_partition_table(storage.GPT)

      self.assertEqual(sda.get_sid(), 42)
      self.assertEqual(gpt.get_sid(), 43)

      tmp1 = devicegraph.find_device(42)
      self.assertTrue(storage.to_disk(tmp1))
      self.assertFalse(storage.to_partition_table(tmp1))

      tmp2 = devicegraph.find_device(43)
      self.assertTrue(storage.to_partition_table(tmp2))
      self.assertFalse(storage.to_disk(tmp2))


if __name__ == '__main__':
    unittest.main()