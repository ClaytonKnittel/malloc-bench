"""Macros for handling compressed files."""

def _remove_suffix(s, suffix):
    if not s.endswith(suffix):
        fail("input must end in " + suffix)
    return s[:-len(suffix)]

def _gz_inflated_file(src):
    out = _remove_suffix(src, ".gz")
    native.genrule(
        name = "gunzip." + src,
        srcs = [src],
        outs = [out],
        cmd_bash = "gunzip -c \"$(<)\" > \"$(@)\"",
    )
    return out

def gz_inflated_filegroup(name, srcs, visibility = None):
    """A filegroup where all srcs are decompressed."""
    native.filegroup(
        name = name,
        srcs = [_gz_inflated_file(src) for src in srcs],
        visibility = visibility,
    )
