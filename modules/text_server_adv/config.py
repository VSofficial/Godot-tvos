def can_build(env, platform):
    # For CoreText, we don't need FreeType dependency on macOS
    if env.get("coretext", False) and platform == "macos":
        env.module_add_dependencies("text_server_adv", ["msdfgen", "svg"], True)
    else:
        env.module_add_dependencies("text_server_adv", ["freetype", "msdfgen", "svg"], True)
    return True


def get_opts(platform):
    from SCons.Variables import BoolVariable

    opts = [
        BoolVariable("graphite", "Enable SIL Graphite smart fonts support", True),
    ]
    
    if platform == "macos":
        opts.append(BoolVariable("coretext", "Use Apple's CoreText instead of FreeType for font rendering", False))
    
    return opts


def configure(env):
    pass


def get_doc_classes():
    return [
        "TextServerAdvanced",
    ]


def get_doc_path():
    return "doc_classes"
