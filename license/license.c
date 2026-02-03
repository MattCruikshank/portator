#include <stdio.h>
#include "portator.h"

static void section(const char *name) {
    printf("────────────────────────────────────────────────────────────\n");
    printf("  %s\n", name);
    printf("────────────────────────────────────────────────────────────\n\n");
}

int main(void) {
    char ver[64];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("%s — Licenses\n\n", ver);

    section("Portator");
    printf(
        "ISC License\n"
        "\n"
        "Copyright 2026 Matthew Cruikshank\n"
        "Copyright 2022 Justine Alexandra Roberts Tunney\n"
        "\n"
        "Permission to use, copy, modify, and/or distribute this software for\n"
        "any purpose with or without fee is hereby granted, provided that the\n"
        "above copyright notice and this permission notice appear in all copies.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL\n"
        "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED\n"
        "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE\n"
        "AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL\n"
        "DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR\n"
        "PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER\n"
        "TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
        "PERFORMANCE OF THIS SOFTWARE.\n\n"
    );

    section("Blink — x86-64 Emulator");
    printf(
        "ISC License\n"
        "\n"
        "Copyright 2022 Justine Alexandra Roberts Tunney\n"
        "\n"
        "Permission to use, copy, modify, and/or distribute this software for\n"
        "any purpose with or without fee is hereby granted, provided that the\n"
        "above copyright notice and this permission notice appear in all copies.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL\n"
        "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED\n"
        "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE\n"
        "AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL\n"
        "DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR\n"
        "PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER\n"
        "TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
        "PERFORMANCE OF THIS SOFTWARE.\n\n"
    );

    section("Cosmopolitan — Portable C Toolchain");
    printf(
        "ISC License\n"
        "\n"
        "Copyright 2020 Justine Alexandra Roberts Tunney\n"
        "\n"
        "Permission to use, copy, modify, and/or distribute this software for\n"
        "any purpose with or without fee is hereby granted, provided that the\n"
        "above copyright notice and this permission notice appear in all copies.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL\n"
        "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED\n"
        "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE\n"
        "AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL\n"
        "DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR\n"
        "PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER\n"
        "TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
        "PERFORMANCE OF THIS SOFTWARE.\n\n"
    );

    section("cJSON — JSON Parser for C");
    printf(
        "MIT License\n"
        "\n"
        "Copyright (c) 2009-2017 Dave Gamble and cJSON contributors\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be included in\n"
        "all copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n"
        "THE SOFTWARE.\n\n"
    );

    section("mustach — Mustache Templates for C");
    printf(
        "0BSD License\n"
        "\n"
        "Copyright (c) Jose Bollo <jobol@nonadev.net>\n"
        "\n"
        "Permission to use, copy, modify, and/or distribute this software for any\n"
        "purpose with or without fee is hereby granted.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
        "WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
        "MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY\n"
        "SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
        "WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION\n"
        "OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN\n"
        "CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n\n"
    );

    section("CivetWeb — HTTP/WebSocket Server");
    printf(
        "MIT License\n"
        "\n"
        "Copyright (c) 2013-2021 the Civetweb developers\n"
        "Copyright (c) 2004-2013 Sergey Lyubka\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be included in\n"
        "all copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n"
        "THE SOFTWARE.\n\n"
    );

    return 0;
}
