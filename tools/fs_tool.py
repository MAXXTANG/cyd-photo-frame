"""PlatformIO's packaged legacy mklittlefs needs a native override on ARM Macs."""
import os
from pathlib import Path
Import("env")
native = os.environ.get("PHOTO_MKLITTLEFS")
if native:
    tool = Path(native).resolve()
    if not tool.is_file() or not os.access(tool, os.X_OK):
        raise RuntimeError("PHOTO_MKLITTLEFS must name an executable")
    env.Replace(MKFSTOOL=str(tool))
