# Generate the Features.h file
# Usage: gen_features.py feature1 feature2 feature3
import os
import sys

with open('src/Features.h', 'w') as f:
    f.write(f"""#pragma once
    /******************************************************************************
    *** ****  WARNING **** ****

    This file is auto-generated.  Any changes made here will be destroyed during
    the next build.  To make persistent changes, edit the template in
    /scripts/gen_features.py
    ******************************************************************************/
    #include <string.h>

    namespace Config {{
        namespace Feature {{
            /**
             * @brief String representation of the enabled flags
             */
            constexpr auto flagString = "{",".join(sys.argv[1:])}";

            /**
             * @brief Compile time checking for enabled flag
             * @param flag - Flag to check
             */
            constexpr bool hasFeature(const char* flag) {{
    """)

    for flag in sys.argv[1:]:
        f.write(f"""\t\tif(strcmp(flag, "{flag}") == 0) return true;\n""")


    f.write(f"""
                return false;
            }}
        }}
    }};
    """)
