# Local image storage

User images are intentionally excluded from the public repository.

Build the empty filesystem for first setup, then upload images through the web
manager. To generate optional original demo images:

```sh
python tools/generate_demo_images.py --output data
```

Images use CYD1 RGB565 format; uploaded images and Wi-Fi credentials are not
committed. Wi-Fi credentials are stored in the device's NVS, outside this folder.
