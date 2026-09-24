# Fase 0 (MAP_PRO.md): formato canónico CMake. Verificado con
# cmake-format 0.6.13 (pip: `pip install cmake-format==0.6.13`).
# Estilo del repo: indentación de 4 espacios, paréntesis de cierre en su
# propia línea sin "dangle".
# ----------------------------------
# Options affecting formatting.
# ----------------------------------
with section("format"):
    line_width = 100
    tab_size = 4
    max_subgroups_hwrap = 3
    max_pargs_hwrap = 4
    separate_ctrl_name_with_space = False
    separate_fn_name_with_space = False
    dangle_parens = True
    dangle_align = "prefix"
    min_prefix_chars = 4
    max_prefix_chars = 10
    max_lines_hwrap = 2
    line_ending = "unix"
    command_case = "lower"
    keyword_case = "upper"
    always_wrap = []
    enable_sort = True
    autosort = False
    require_valid_layout = False

# ----------------------------------
# Options affecting comment reflow and formatting.
# ----------------------------------
with section("markup"):
    bullet_char = "*"
    enum_char = "."
    enable_markup = False

# ----------------------------------
# Options affecting parse.
# ----------------------------------
with section("parse"):
    pass
