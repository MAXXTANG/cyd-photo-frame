import importlib.util
from pathlib import Path
import struct
import unittest
import zlib

spec=importlib.util.spec_from_file_location("usb_image",Path(__file__).resolve().parents[1]/"tools/replace_usb_image.py")
usb=importlib.util.module_from_spec(spec);spec.loader.exec_module(usb)


class ImageValidation(unittest.TestCase):
    def test_dimensions_crc_and_exact_length(self):
        pixels=bytes(153600)
        image=struct.pack("<4sHHII",b"CYD1",320,240,len(pixels),zlib.crc32(pixels))+pixels
        self.assertEqual(usb.validate(image)[:2],(320,240))
        for bad in (image[:-1],image+b"x",image[:16]+b"x"+image[17:],b"XXXX"+image[4:]):
            with self.assertRaises(ValueError):usb.validate(bad)


if __name__=="__main__":unittest.main()
