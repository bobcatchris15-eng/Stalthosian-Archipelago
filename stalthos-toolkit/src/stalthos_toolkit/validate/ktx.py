"""Container sniffing for texture format acceptance checks.

Dependency-free and Blender-free: we read a few header bytes and map a small,
explicit set of Vulkan/BC format codes. Anything we cannot identify is reported
as `unknown`, never guessed -- an uncompressed texture must never be published
silently (docs/toolkit-architecture.md section 6).
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

# KTX2 file identifier: AB 4B 54 58 20 31 31 BB 0D 0A 1A 0A
KTX2_IDENTIFIER = bytes((0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A))

# Vulkan block-compressed format numbers we care about (BC1..BC7).
VK_BC_FORMATS: dict[int, str] = {
    131: "BC1_RGB_UNORM",
    132: "BC1_RGBA_UNORM",
    133: "BC2_UNORM",
    134: "BC2_SRGB",
    135: "BC3_UNORM",
    136: "BC3_SRGB",
    137: "BC4_UNORM",
    138: "BC4_SNORM",
    139: "BC5_UNORM",
    140: "BC5_SNORM",
    141: "BC6H_UF16",
    142: "BC6H_SF16",
    145: "BC7_UNORM",
    146: "BC7_SNORM",
    147: "BC7_UNORM_SRGB",
    148: "BC7_SNORM_SRGB",
}

#: Normal maps want two-channel block compression; colour wants BC7 when it fits.
NORMAL_FORMATS = frozenset({"BC5_UNORM", "BC5_SNORM"})
COLOR_FORMATS = frozenset({"BC7_UNORM", "BC7_SNORM", "BC7_UNORM_SRGB", "BC7_SNORM_SRGB"})

_HEADER_BYTES = 32


@dataclass(frozen=True)
class TextureFormatInfo:
    container: str            # ktx2 | png | tga | dds | exr | jpeg | unknown
    codec: str | None         # e.g. "BC5_UNORM", or None when not compressed/known
    width: int | None = None
    height: int | None = None
    readable: bool = True

    def to_dict(self) -> dict:
        return {
            "container": self.container,
            "codec": self.codec,
            "width": self.width,
            "height": self.height,
        }


def sniff_texture(path: str | Path) -> TextureFormatInfo:
    """Identify a texture container/format from its header, offline."""
    path = Path(path)
    try:
        with open(path, "rb") as fh:
            head = fh.read(_HEADER_BYTES)
    except OSError:
        return TextureFormatInfo("unknown", None, readable=False)

    if head.startswith(KTX2_IDENTIFIER) and len(head) >= 24:
        vk_format = struct.unpack_from("<I", head, 12)[0]
        width, height = 0, 0
        # levelIndex/.. then pixelWidth at 20, pixelHeight at 24 (needs 28 bytes)
        if len(head) >= 28:
            width, height = struct.unpack_from("<II", head, 20)
        return TextureFormatInfo("ktx2", VK_BC_FORMATS.get(vk_format), width, height)

    if head.startswith(b"\x89PNG\r\n\x1a\n") and len(head) >= 24:
        width, height = struct.unpack_from(">II", head, 16)
        return TextureFormatInfo("png", None, width, height)

    if head.startswith(b"RIFF") and head[8:12] == b"WEBP":
        return TextureFormatInfo("webp", None)

    if head.startswith(b"\xff\xd8\xff"):
        return TextureFormatInfo("jpeg", None)

    if head.startswith(b"DDS "):
        return TextureFormatInfo("dds", None)

    if head[:4] in (b"\x76\x2f\x31\x01", b"\x76\x2f\x31\x81"):
        return TextureFormatInfo("exr", None)

    if head.startswith(b"\x00\x00\x00\x00") and len(head) >= 18:
        # TGA has no magic; an uncompressed 2-byte truecolour header starts with 0.
        return TextureFormatInfo("tga", None)

    return TextureFormatInfo("unknown", None)


def is_compressed(info: TextureFormatInfo) -> bool:
    return info.codec is not None and info.codec.startswith("BC")