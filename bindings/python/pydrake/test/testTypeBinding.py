from __future__ import print_function, absolute_import

import unittest
import numpy as np
import pydrake
from pydrake import typebinding as tb

class TestTypeBinding(unittest.TestCase):
    def test_basic(self):
        obj = tb.SimpleType(1)
        self.assertEqual(obj.value(), 1)

if __name__ == '__main__':
    unittest.main()
